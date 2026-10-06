// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#if !defined (__thekogans_util_SlabAllocatorDetail_h)
#define __thekogans_util_SlabAllocatorDetail_h

#include <cstddef>
#include <new>
#include <type_traits>
#include "thekogans/util/Config.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/CPU.h"

namespace thekogans {
    namespace util {

        namespace detail {
            /// \brief
            /// Default slots per page. A tuning knob meant to limit page
            /// allocations for a heavily allocated type.
            static constexpr std::size_t DEFAULT_SLOTS_PER_PAGE = 512;
            /// \brief
            /// Default Thread Local Cache (TLC) size.
            /// A tuning knob meant to limit lock contention on a heavily contested type.
            static constexpr std::size_t DEFAULT_TLC_THRESHOLD = 64;

            // Our target harvest is slots / threads.
            constexpr std::size_t CalculateThreshold (
                    std::size_t slots,
                    std::size_t threads) noexcept {
                // Target harvest per thread = slots / threads
                std::size_t harvest = slots / threads;
                std::size_t threshold = harvest * 2;
                // Apply the floor clamp.
                if (threshold < (threads / 2)) {
                    threshold = (threads / 2);
                }
                // Apply the cascading safety cap.
                if ((threshold / 2) >= slots) {
                    threshold = slots;
                }
                return threshold;
            }
        }

        namespace Policy {
            /// \brief
            /// This is a very interesting policy because it defines the allocator's tear down
            /// behavior. If IsSingleton == false (the default), the allocator will execute a
            /// dtor reclaiming all contained pages back to the PageAllocator. Together with the
            /// PageAllocator policy, this policy gives you the ability to build tennant allocators.
            /// Tenant allocators borrow resources. They don't own them. By passing
            /// Policy::IsSingleton<true> to your scoped allocators (\see{ScopedSlabAllocator}),
            /// combined with a Policy::PageAllocator<Type> (Default is DefaultPageAllocator) that
            /// will outlive the tenant, you create a performant allocator that doesn't waste CPU
            /// cycles.
            template<bool Value>
            struct IsSingleton {
                static constexpr bool value = Value;
            };
            /// \brief
            /// Thread Local Cache size.
            template<std::size_t Value>
            struct TLCThreshold {
                static constexpr std::size_t value = Value;
            };
            template<std::size_t Value>
            struct SlotsPerPage {
                static constexpr std::size_t value = Value;
            };
            template<std::size_t Value>
            struct CacheLineSize {
                static constexpr std::size_t value = Value;
            };
            template<std::size_t Value>
            struct Id {
                static constexpr std::size_t value = Value;
            };
            template<bool Value>
            struct IsCompaction {
                static constexpr bool value = Value;
            };

            // Type policies hold the raw type definition
            template<typename T>
            struct Lock {
                using type = T;
            };
            template<typename T>
            struct PageAllocator {
                using type = T;
            };
            /// \brief
            /// This policy is not used by the SlabAllocator. It is used by the
            /// \see{GlobalSlabAllocator} (SlabAllocator.h) as a template parameter
            /// for \see{Singleton} (from which it is derived). If you're deriving
            /// your allocator from SlabAllocator (aka ScopedSlabAllocator), it is
            /// completely ignored.
            template <template <typename> typename T>
            struct InstanceCreator {
                template <typename U> using template_type = T<U>;
            };

            /// \brief
            /// Policy helper to calculate an optimal TLC threshold.
            /// \param SlotsPerPage The configured slots per page boundary.
            /// \param ThreadDensity The number of threads sharing this allocator.
            template<
                std::size_t SlotsPerPage,
                std::size_t ThreadDensity = 16>
            struct DeriveTLCThreshold {
                static constexpr std::size_t TLCThreshold =
                    detail::CalculateThreshold (SlotsPerPage, ThreadDensity);
            };

            /// \brief
            /// Policy helper to calculate an optimal slots per page.
            /// \param TLCThreshold The configured thread local cache boundary.
            /// \param ThreadDensity The number of threads sharing this allocator.
            template<
                std::size_t TLCThreshold,
                std::size_t ThreadDensity = 16>
            struct DeriveSlotsPerPage {
                static constexpr std::size_t SlotsPerPage = TLCThreshold * ThreadDensity;
            };

            /// \brief
            /// Policy helper to calculate an optimal slots per page and tlc threshold.
            /// \param ThreadDensity The number of threads sharing this allocator.
            template<std::size_t ThreadDensity = 16>
            struct DeriveSlotsPerPageAndTLCThreshold {
                static constexpr std::size_t SlotsPerPage =
                    DeriveSlotsPerPage<detail::DEFAULT_TLC_THRESHOLD, ThreadDensity>::SlotsPerPage;
                static constexpr std::size_t TLCThreshold = detail::DEFAULT_TLC_THRESHOLD;
            };
        }

        namespace detail {
            /// \brief
            /// Forward declaration needed by the GetInstanceCreatorPolicy extractor below.
            template<typename T, typename... Policies>
            struct SlabAllocator;

            // --- bool EXTRACTOR MECHANICS ---
            template<template<bool> typename TargetPolicy, bool DefaultValue, typename... Policies>
            struct GetBoolPolicy {
                static constexpr bool value = DefaultValue; // Base case: fallback to default
            };

            template<template<bool> typename TargetPolicy, bool DefaultValue, bool CurrentValue, typename... Rest>
            struct GetBoolPolicy<TargetPolicy, DefaultValue, TargetPolicy<CurrentValue>, Rest...> {
                static constexpr bool value = CurrentValue; // Found it: extract the template value
            };

            template<template<bool> typename TargetPolicy, bool DefaultValue, typename Head, typename... Rest>
            struct GetBoolPolicy<TargetPolicy, DefaultValue, Head, Rest...> {
                static constexpr bool value = GetBoolPolicy<TargetPolicy, DefaultValue, Rest...>::value; // Skip and keep looking
            };

            // --- std::size_t EXTRACTOR MECHANICS ---
            template<template<std::size_t> typename TargetPolicy, std::size_t DefaultValue, typename... Policies>
            struct GetValuePolicy {
                static constexpr std::size_t value = DefaultValue; // Base case: fallback to default
            };

            template<template<std::size_t> typename TargetPolicy, std::size_t DefaultValue, std::size_t CurrentValue, typename... Rest>
            struct GetValuePolicy<TargetPolicy, DefaultValue, TargetPolicy<CurrentValue>, Rest...> {
                static constexpr std::size_t value = CurrentValue; // Found it: extract the template value
            };

            template<template<std::size_t> typename TargetPolicy, std::size_t DefaultValue, typename Head, typename... Rest>
            struct GetValuePolicy<TargetPolicy, DefaultValue, Head, Rest...> {
                static constexpr std::size_t value = GetValuePolicy<TargetPolicy, DefaultValue, Rest...>::value; // Skip and keep looking
            };

            // --------------------------------------------------------------------
            // Unpacker for SlotsPerPage
            // --------------------------------------------------------------------
            template<std::size_t Default, typename... Policies>
            struct GetSlotsPerPage;

            // Base Case: Empty pack, return default baseline
            template<std::size_t Default>
            struct GetSlotsPerPage<Default> {
                static constexpr std::size_t value = Default;
            };

            // Match Case A: Standard explicit Policy::SlotsPerPage found
            template<std::size_t Default, std::size_t N, typename... Rest>
            struct GetSlotsPerPage<Default, Policy::SlotsPerPage<N>, Rest...> {
                static constexpr std::size_t value = N;
            };

            // Match Case B: Unified Policy::Derive tag found! Dig inside it.
            template <std::size_t Default, std::size_t S, std::size_t T, typename... Rest>
            struct GetSlotsPerPage<Default, Policy::DeriveTLCThreshold<S, T>, Rest...> {
                static constexpr std::size_t value = S; // Pull out the slots configuration
            };

            // Fallthrough Case: Skip unrelated tags
            template<std::size_t Default, typename T, typename... Rest>
            struct GetSlotsPerPage<Default, T, Rest...> {
                static constexpr std::size_t value = GetSlotsPerPage<Default, Rest...>::value;
            };

            // --------------------------------------------------------------------
            // Unpacker for TLCThreshold
            // --------------------------------------------------------------------
            template<std::size_t Default, typename... Policies>
            struct GetTLCThreshold;

            // Base Case: Empty pack, return your default baseline value
            template<std::size_t Default>
            struct GetTLCThreshold<Default> {
                static constexpr std::size_t value = Default;
            };

            // Match Case A: Standard explicit Policy::TLCThreshold found
            template<std::size_t Default, std::size_t N, typename... Rest>
            struct GetTLCThreshold<Default, Policy::TLCThreshold<N>, Rest...> {
                static constexpr std::size_t value = N;
            };

            // Match Case B: Unified Policy::Derive tag found! Pull out the COMPUTED value
            template<std::size_t Default, std::size_t S, std::size_t T, typename... Rest>
            struct GetTLCThreshold<Default, Policy::DeriveTLCThreshold<S, T>, Rest...> {
                static constexpr std::size_t value = Policy::DeriveTLCThreshold<S, T>::TLCThreshold;
            };

            // Fallthrough Case: Skip unrelated tags
            template<std::size_t Default, typename T, typename... Rest>
            struct GetTLCThreshold<Default, T, Rest...> {
                static constexpr std::size_t value = GetTLCThreshold<Default, Rest...>::value;
            };

            // --- TYPE EXTRACTOR MECHANICS ---
            template<template<typename> typename TargetPolicy, typename DefaultType, typename... Policies>
            struct GetTypePolicy {
                using type = DefaultType;
            };

            template<template<typename> typename TargetPolicy, typename DefaultType, typename CurrentType, typename... Rest>
            struct GetTypePolicy<TargetPolicy, DefaultType, TargetPolicy<CurrentType>, Rest...> {
                using type = CurrentType;
            };

            template<template<typename> typename TargetPolicy, typename DefaultType, typename Head, typename... Rest>
            struct GetTypePolicy<TargetPolicy, DefaultType, Head, Rest...> {
                using type = typename GetTypePolicy<TargetPolicy, DefaultType, Rest...>::type;
            };

            // --- InstanceCreator EXTRACTOR MECHANICS ---
            // Base Case: Fallback to the engine's default allocator creator if the tag is missing
            template<typename T, typename DefaultType, typename... Policies>
            struct GetInstanceCreatorPolicyHelper {
                using type = DefaultType;
            };

            // Match Case: If we encounter the InstanceCreator type tag wrapper, extract its template type!
            template<typename T, typename DefaultType, template <typename> typename CurrentTemplate, typename... Rest>
            struct GetInstanceCreatorPolicyHelper<T, DefaultType, Policy::InstanceCreator<CurrentTemplate>, Rest...> {
                using type = typename Policy::InstanceCreator<CurrentTemplate>::template template_type<T>;
            };

            // Traversal Case: Skip unrelated tags and keep looking down the pack
            template<typename T, typename DefaultType, typename Head, typename... Rest>
            struct GetInstanceCreatorPolicyHelper<T, DefaultType, Head, Rest...> {
                using type = typename GetInstanceCreatorPolicyHelper<T, DefaultType, Rest...>::type;
            };

            // Publicly exposed extractor helper mapping for your main inheritance block
            template<typename T, typename... Policies>
            struct GetInstanceCreatorPolicy {
                using type = typename GetInstanceCreatorPolicyHelper<
                    SlabAllocator<T, Policies...>,
                    DefaultInstanceCreator<SlabAllocator<T, Policies...>>,
                    Policies...>::type;
            };

            // --- SlabAllocator policy validation static_asssert helpers.
            // A compile-time trait to check if a type is a valid policy for our allocator
            template<typename P>
            struct IsValidSlabPolicy : std::false_type {};
            // Explicitly whitelist every supported tag right here in the detail namespace!
            template<bool V>
            struct IsValidSlabPolicy<Policy::IsSingleton<V>> : std::true_type {};
            template<std::size_t V>
            struct IsValidSlabPolicy<Policy::SlotsPerPage<V>> : std::true_type {};
            template<std::size_t V>
            struct IsValidSlabPolicy<Policy::TLCThreshold<V>> : std::true_type {};
            template<std::size_t V>
            struct IsValidSlabPolicy<Policy::CacheLineSize<V>> : std::true_type {};
            template<std::size_t V>
            struct IsValidSlabPolicy<Policy::Id<V>> : std::true_type {};
            template<bool V>
            struct IsValidSlabPolicy<Policy::IsCompaction<V>> : std::true_type {};
            template<typename L>
            struct IsValidSlabPolicy<Policy::Lock<L>> : std::true_type {};
            template<typename A>
            struct IsValidSlabPolicy<Policy::PageAllocator<A>> : std::true_type {};
            template<template <typename> typename C>
            struct IsValidSlabPolicy<Policy::InstanceCreator<C>> : std::true_type {};
            template <std::size_t SlotsPerPage, std::size_t ThreadDensity>
            struct IsValidSlabPolicy<Policy::DeriveTLCThreshold<SlotsPerPage, ThreadDensity>> : std::true_type {};
            template <std::size_t TLCThreshold, std::size_t ThreadDensity>
            struct IsValidSlabPolicy<Policy::DeriveSlotsPerPage<TLCThreshold, ThreadDensity>> : std::true_type {};
            template <std::size_t ThreadDensity>
            struct IsValidSlabPolicy<Policy::DeriveSlotsPerPageAndTLCThreshold<ThreadDensity>> : std::true_type {};
            // Helper to evaluate the entire variadic pack at once
            template <typename... Policies>
            constexpr bool ValidatePolicies () {
                return (IsValidSlabPolicy<Policies>::value && ...);
            }

            /// \struct DefaultPageAllocator SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
            ///
            /// \brief
            /// The default page allocator. Yet another tuning knob to allow you to
            /// control how pages are allocated. This one uses direct OS services to
            /// return aligned and clean (0 filled) pages.
            struct _LIB_THEKOGANS_UTIL_DECL DefaultPageAllocator {
                /// \brief
                /// Allocate a pageSize aligned and 0 filled page.
                /// \param[in] pageSize Page size and alignement.
                /// \return pageSize aligned and 0 filled page.
                static void *Alloc (std::size_t pageSize) noexcept;
                /// \brief
                /// Free a previously Alloc'ed page.
                /// \param[in] ptr Page pointer returned by Alloc above.
                /// \param[in] pageSize Same pageSize you passed to Alloc.
                static void Free (
                    void *ptr,
                    std::size_t pageSize) noexcept;
            };

            /// \struct StdPageAllocator SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
            ///
            /// \brief
            /// If the DefaultPageAllocator proves to be a bottleneck on your system,
            /// use StdPageAllocator. It uses operator new and delete and zero fills
            /// the buffers they return and release.
            struct _LIB_THEKOGANS_UTIL_DECL StdPageAllocator {
                /// \brief
                /// Allocate a pageSize aligned and 0 filled page.
                /// \param[in] pageSize Page size and alignement.
                /// \return pageSize aligned and 0 filled page.
                static void *Alloc (std::size_t pageSize) noexcept;
                /// \brief
                /// Free a previously Alloc'ed page.
                /// \param[in] ptr Page pointer returned by Alloc above.
                /// \param[in] pageSize Same pageSize you passed to Alloc.
                static void Free (
                    void *ptr,
                    std::size_t pageSize) noexcept;
            };

            /// \struct SlotSize SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
            ///
            /// \brief
            /// Compile time template to calculate the slot size for a given type T.
            template<typename T>
            struct SlotSize {
                static constexpr std::size_t calcSlotSize () noexcept {
                    // 1. Determine the maximum alignment required by either the object or the free list pointer
                    constexpr std::size_t requiredAlign = MAX (alignof (T), alignof (void *));
                    // 2. Determine the maximum size required by either object
                    constexpr std::size_t rawSize = MAX (sizeof (T), sizeof (void *));
                    // 3. Round up the size to a clean multiple of our highest alignment requirement
                    return (rawSize + requiredAlign - 1) / requiredAlign * requiredAlign;
                }
                static constexpr std::size_t value = calcSlotSize ();
            };

            /// \struct SlabAllocator SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
            ///
            /// \brief
            /// A SlabAllocator's purpose is to allocate memory for a single type. Knowing the size
            /// of the type it's allocating for a priori makes all the difference in the world. We
            /// can use that information to design a highly efficient, specifically tuned allocation
            /// engine. This one is designed with all the bells and whistles and to run in 'true' O(1)
            /// time complexity save for the caveat that PageAllocator will do what it will do. As you
            /// can see it's template takes up to 8! parameters. All but the first are defaulted with
            /// sensible values so that you can just leave them alone 90% of the time. But when those
            /// critical corner cases and specialty conditions demand it, it can be tuned to fit any
            /// environment and need. Great care has been taken to make sure alignment requirements
            /// are not only met but are enforced to make this code as performant as possible on modern
            /// cache line driven architectures. Key variables have been isolated in to their own cache
            /// lines to further protect against false sharing. By utilizing it's tuning knobs one can
            /// build highly specific, lock free (NullLock) allocators that execute their Alloc and Free
            /// in just a small handful of machine instructions. The use of SlabAllocator for your
            /// types also greatly aids in avoiding global heap fragmentation as types are allocated
            /// from contiguous pages.
            ///
            /// SlabAllocator is my first collaboration with an AI (Google's Gemini). I posed a question;
            /// Is there a practical way to implement a slab allocator with 'true' O(1) performance
            /// guarantees? I didn't want average or amortized O(1). I wanted true O(1). After many
            /// iterations and blind alley ventures, this is what we both came up with. The architecture
            /// is all mine. The nuts and bolts of alignment, constexpr and TLC are all AI's. This
            /// implementation evolved over just two days of back and forth. Looking back on my earlier
            /// efforts I can honestly say that to achieve this level of sophistication in the past would
            /// take me significantly longer. Digging through old chats. Looking for exact documentation
            /// I needed would have consumed an enormous amount of time and effort. On top of all this,
            /// once we were done designing, and I was done implementing it took the AI mere seconds to
            /// generate a burn the earth down validation suite. Again, something that would take me a
            /// day or two to do by myself. All in all, I am absolutely sold on the idea of pair programming
            /// with AI. To be sure it's not all roses. There were times it was missing context and tried
            /// to lead me down blind alleys. But that's why it's a collaboration. It's not an all knowing,
            /// all seeing oracle that will flawlessly do your work for you. It's a fantastically powerful
            /// tool that in the right hands creates an unbeatable team.
            ///
            /// \tparam T The type that we are allocating for.
            /// \tparam Policies A pack of policies you want to modify. The order is irrelevant!
            template <typename T, typename... Policies>
            struct SlabAllocator {
            private:
                static_assert (ValidatePolicies<Policies...> (),
                    "=========================================================================\n"
                    "  CRITICAL ERROR: Invalid or Typoed Policy Tag passed to SlabAllocator.\n"
                    "=========================================================================\n"
                    "  Supported Allocator Policies are exclusively:\n"
                    "    - Policy::IsSingleton<bool> (Default false)\n"
                    "    - Policy::SlotsPerPage<std::size_t> (Default detail::DEFAULT_SLOTS_PER_PAGE)\n"
                    "    - Policy::TLCThreshold<std::size_t> (Default detail::DEFAULT_TLC_THRESHOLD)\n"
                    "    - Policy::CacheLineSize<std::size_t> (Default SYSTEM_CACHE_LINE_SIZE)\n"
                    "    - Policy::Id<std::size_t> (Default 0)\n"
                    "    - Policy::Lock<typename> (Default SpinLock)\n"
                    "    - Policy::PageAllocator<typename> (Default detail::DefaultPageAllocator\n"
                    "    - Policy::InstanceCreator<template> (only used if derived from GlobalSlabAllocator)\n"
                    "    - Policy::IsCompaction<bool> (Default false)\n"
                    "    - Policy::DeriveTLCThreshold<std::size_t, std::size_t>\n"
                    "    - Policy::DeriveSlotsPerPage<std::size_t, std::size_t>\n"
                    "    - Policy::DeriveSlotsPerPageAndTLCThreshold<std::size_t>\n"
                    "=========================================================================\n");

                static constexpr bool IsSingleton = GetBoolPolicy<
                    Policy::IsSingleton, false, Policies...>::value;
                static constexpr std::size_t TLCThreshold = GetTLCThreshold<
                    DEFAULT_TLC_THRESHOLD, Policies...>::value;
                static constexpr std::size_t SlotsPerPage = GetSlotsPerPage<
                    DEFAULT_SLOTS_PER_PAGE, Policies...>::value;
                static constexpr std::size_t CacheLineSize = GetValuePolicy<
                    Policy::CacheLineSize, SYSTEM_CACHE_LINE_SIZE, Policies...>::value;
                static constexpr std::size_t Id = GetValuePolicy<
                    Policy::Id, 0, Policies...>::value;
                static constexpr bool CompressOnZero = GetBoolPolicy<
                    Policy::IsCompaction, true, Policies...>::value;
                // Extract policy types.
                using Lock = typename GetTypePolicy<
                    Policy::Lock, SpinLock, Policies...>::type;
                using PageAllocator = typename GetTypePolicy<
                    Policy::PageAllocator, DefaultPageAllocator, Policies...>::type;

                /// \brief
                /// Validate template parameters.
                static_assert (SlotsPerPage > 0, "SlotsPerPage must be > 0.");
                static_assert ((TLCThreshold / 2) <= SlotsPerPage,
                    "CRITICAL CONFIGURATION ERROR: Your TLC batch target (TLCThreshold / 2) "
                    "is larger than the maximum slots available in a single page (SlotsPerPage). "
                    "This would cause a single thread allocation burst to swallow and evict "
                    "multiple entire pages instantly, causing severe memory fragmentation. "
                    "Please decrease TLCThreshold or increase SlotsPerPage.");
                static_assert (IsPowerOf2 (CacheLineSize), "CacheLineSize must be a power of 2.");

                static constexpr std::size_t calcSlotSize () noexcept {
                    // 1. Determine the maximum alignment required by either the object or the free list pointer
                    constexpr std::size_t requiredAlign = std::max (alignof (T), alignof (typename Page::Slot));
                    // 2. Determine the maximum size required by either object
                    constexpr std::size_t rawSize = std::max (sizeof (T), sizeof (typename Page::Slot));
                    // 3. Round up the size to a clean multiple of our highest alignment requirement
                    // This removes the buggy bitwise mask and handles all alignment constraints perfectly
                    return (rawSize + requiredAlign - 1) / requiredAlign * requiredAlign;
                }

                // The following consts are allocator invariants. We calculate them
                // once at compile time and use them at run time as 'magic' numbers.

                /// \brief
                /// Slot size.
                static constexpr std::size_t slotSize = SlotSize<T>::value;
                /// \brief
                /// Page size (header + slots). Aligned to the next power of 2.
                static constexpr std::size_t pageSize = Align (CacheLineSize + slotSize * SlotsPerPage);
                /// \brief
                /// Page mask used to turn raw void * in to Page * (see Free).
                static constexpr std::size_t pageMask = pageSize - 1;
                /// \brief
                /// Maximum slots per page.
                static constexpr std::size_t maxSlots = (pageSize - CacheLineSize) / slotSize;

                /// \struct SlabAllocator::Page SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
                ///
                /// \brief
                /// The page (aka slab) from which we allocate slots. It's aligned
                /// on and occupies an entire cache line to prevent false sharing
                /// and cache thrashing. Pages form a singly linked list rooted in
                /// pageList.
                struct alignas (CacheLineSize) Page {
                    /// \struct SlabAllocator::Page::Slot SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
                    ///
                    /// \brief
                    /// Slot overlays our free slot list on top of released user data.
                    struct Slot {
                        /// \brief
                        /// Free slots form a singly linked list rooted at freeList.
                        /// Pointer to the next free slot in the list.
                        Slot *next;

                        inline void Free () noexcept {
                            Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (this) & ~pageMask);
                            page->Free (this);
                        }
                        inline static bool Free (void *ptr) noexcept {
                            Page *page = reinterpret_cast<Page *> (reinterpret_cast<uintptr_t> (ptr) & ~pageMask);
                            if (page->header.magic == MAGIC64) {
                                page->header.allocator.Free (ptr);
                                return true;
                            }
                            return false;
                        }
                    };
                    /// \struct SlabAllocator::Page::Header SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
                    ///
                    /// \brief
                    /// We put our metadata in to a header to allow the compiler to align
                    /// the fields as it sees fit and to make our padding calculations below
                    /// bulletproof. One of the most important performance knobs is preventing
                    /// false sharing. By padding out the page header to a cache line size we
                    /// guarantee to prevent sharing it with slots.
                    struct Header {
                        ui64 magic = MAGIC64;
                        SlabAllocator &allocator;
                        struct EmptyMasterNext {};
                        using MasterNext = std::conditional_t<!IsSingleton, Page *, EmptyMasterNext>;
                        [[no_unique_address]] MasterNext masterNext = {};
                        /// \brief
                        /// Next page in the list.
                        Page *partialNext{nullptr};
                        /// \brief
                        /// Number of slots allocated from this page.
                        std::size_t slotCount{0};
                        Slot *freeList{nullptr};

                        Header (SlabAllocator &allocator_) noexcept :
                            allocator (allocator_),
                            partialNext (allocator.storage.partialPageList) {}
                    } header;

                    /// \brief
                    /// Calculate the size of the header.
                    static constexpr std::size_t metadataSize = sizeof (Header);
                    // Guard the Metadata Header Block
                    static_assert (
                        metadataSize <= CacheLineSize,
                        "Page metadata header size has exceeded a single CacheLineSize block.");

                    /// \brief
                    /// Calculated padding size.
                    static constexpr std::size_t paddingSize = CacheLineSize - metadataSize;
                    /// \struct SlabAllocator::Page::EmptyPadding SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
                    ///
                    /// \brief
                    /// This empty struct is a conditional placeholder for PaddingType in case paddingSize == 0.
                    struct EmptyPadding {};
                    /// \brief
                    /// Declare padding type to be either std::byte[] if paddingSize > 0, or EmptyPadding if paddingSize == 0.
                    using PaddingType = std::conditional_t<(paddingSize > 0), std::byte[paddingSize], EmptyPadding>;
                    /// \brief
                    /// Declare a conditional PaddingType using a standards compliant (c++17 and >) compiler attribute.
                    /// [[no_unique_address]] ensures EmptyPadding takes up absolutely zero bytes of space!
                    [[no_unique_address]] PaddingType padding = {};

                    /// \brief
                    /// ctor.
                    /// \param[in, out] head Head of the partial list.
                    Page (SlabAllocator &allocator) noexcept :
                            header (allocator) {
                        allocator.storage.partialPageList = this;
                    }

                    /// \brief
                    /// Allocate a slot.
                    /// \return A new slot.
                    inline Slot *Alloc () noexcept {
                        Slot *slot = nullptr;
                        if (header.freeList != nullptr) {
                            slot = header.freeList;
                            header.freeList = header.freeList->next;
                        }
                        else {
                            slot = reinterpret_cast<Slot *> (
                                reinterpret_cast<std::byte *> (this + 1) + header.slotCount * slotSize);
                        }
                        if (THEKOGANS_UTIL_UNLIKELY (++header.slotCount == maxSlots)) {
                            // Page is full. Evict it from the list so that no one asks it
                            // for slots again. Yes the page is now floating out there in
                            // the either completely inaccessible until someone decides to
                            // free one of it's slots or if the allocator is scoped and it's
                            // dtor fires. Either way full pages get evicted from the partial
                            // list so as not to waste time traversion over them when allocating.
                            header.allocator.storage.partialPageList = header.partialNext;
                            header.partialNext = nullptr;
                        }
                        return slot;
                    }

                    /// \brief
                    /// Return a previously Alloc(ated) slot back to the free list.
                    /// \param[in] ptr Slot pointer to free.
                    inline void Free (Slot *slot) noexcept {
                        slot->next = header.freeList;
                        header.freeList = slot;
                        --header.slotCount;
                        if constexpr (CompressOnZero) {
                            if (THEKOGANS_UTIL_UNLIKELY (header.slotCount == 0)) {
                                header.freeList = nullptr;
                            }
                        }
                        // The page transitioned from full to partial.
                        // Wire it back in to our list from the either.
                        if (THEKOGANS_UTIL_UNLIKELY (header.slotCount + 1 == maxSlots)) {
                            header.partialNext = header.allocator.storage.partialPageList;
                            header.allocator.storage.partialPageList = this;
                        }
                    }
                };

                // Validate our assumptions and perform sanity checks.
                static_assert (
                    sizeof (Page) == CacheLineSize,
                    "sizeof (Page) must be EXACTLY equal to CacheLineSize.");

                struct ScopedSlabAllocatorStorageBase {
                    Page *masterPageList{nullptr};
                    ~ScopedSlabAllocatorStorageBase () noexcept {
                        Page *page = masterPageList;
                        while (page != nullptr) {
                            Page *next = page->header.masterNext;
                            PageAllocator::Free (page, pageSize);
                            page = next;
                        }
                    }
                };
                struct SingletonSlabAllocatorStorageBase {};
                using StorageBase = std::conditional_t<
                    !IsSingleton,
                    ScopedSlabAllocatorStorageBase,
                    SingletonSlabAllocatorStorageBase>;

                struct Storage : public StorageBase {
                    /// \brief
                    /// Partially allocated page list.
                    Page *partialPageList{nullptr};
                    /// \brief
                    /// Flag to protect multiple threads from calling the page allocator.
                    bool pageAllocationInFlight{false};
                    /// \brief
                    /// Protect access to storage.
                    /// Align the lock to it's own cache line to prevent false sharing with pageList.
                    alignas (CacheLineSize) Lock lock;
                } storage;

                /// \struct SlabAllocator::TLC SlabAllocatorDetail.h thekogans/util/SlabAllocatorDetail.h
                ///
                /// \brief
                /// Thread Local Cache (TLC). We keep a small (TLCThreshold) number of slots
                /// per thread. This optimization allows us to bypass the costly lock
                /// acquisition. In real load testing (see test_SlabAllocator) this
                /// results in ~65% speedup!
                struct TLC {
                    /// \brief
                    /// Our own local slot cache.
                    typename Page::Slot *slotList{nullptr};
                    /// \brief
                    /// Number of slots currently in the cache.
                    std::size_t slotCount{0};

                    /// \brief
                    /// Add a slot to the local cache.
                    /// \param[in] slot Page::Slot to add to the cache.
                    inline void Push (typename Page::Slot *slot) noexcept {
                        slot->next = slotList;
                        slotList = slot;
                        ++slotCount;
                    }

                    /// \brief
                    /// Remove a slot from the cache.
                    /// \return Head of the cache list.
                    inline typename Page::Slot *Pop () noexcept {
                        typename Page::Slot *slot = slotList;
                        slotList = slotList->next;
                        --slotCount;
                        return slot;
                    }
                };

                /// \brief
                /// Return the TLC.
                /// \return tlc.
                static TLC &GetTLC () noexcept {
                    thread_local TLC tlc;
                    return tlc;
                }

            public:
                /// \brief
                /// Allocate a new slot.
                void *Alloc () {
                    void *ptr = nullptr;
                    if constexpr (TLCThreshold > 0) {
                        TLC &tlc = GetTLC ();
                        // See if we have a free slot in our local cache.
                        if (THEKOGANS_UTIL_UNLIKELY (tlc.slotList == nullptr)) {
                            // No banana. Let's seed the TLC.
                            static constexpr std::size_t batchTarget = TLCThreshold / 2;
                            // Persistent outer loop forces lock retention until TLC target is met.
                            LockGuard<Lock> guard (storage.lock);
                            while (tlc.slotCount == 0 /*< batchTarget*/) {
                                // Inner loop aggressively drains whatever pages are currently available.
                                while (tlc.slotCount < batchTarget && storage.partialPageList != nullptr) {
                                    tlc.Push (storage.partialPageList->Alloc ());
                                }
                                // If inner loop broke but target isn't met, the pool is dry.
                                // Seed a fresh page from the OS and let the outer loop repeat the harvest.
                                if (tlc.slotCount == 0 /*< batchTarget*/) {
                                    AllocPage ();
                                }
                            }
                        }
                        ptr = tlc.Pop ();
                    }
                    else {
                        // TLC compiled out: Single clean allocation tracking.
                        LockGuard<Lock> guard (storage.lock);
                        if (THEKOGANS_UTIL_UNLIKELY (storage.partialPageList == nullptr)) {
                            AllocPage ();
                        }
                        ptr = storage.partialPageList->Alloc ();
                    }
                    return ptr;
                }

                /// \brief
                /// Try to cache a previously allocated slot. If our cache is full,
                /// return half back to the pages the slots came from.
                void Free (void *ptr) noexcept {
                    if (THEKOGANS_UTIL_LIKELY (ptr != nullptr)) {
                        typename Page::Slot *slot = reinterpret_cast<typename Page::Slot *> (ptr);
                        if constexpr (TLCThreshold > 0) {
                            TLC &tlc = GetTLC ();
                            tlc.Push (slot);
                            // If we have no more room in our local cache batch release a bunch
                            // of slots back to their pages so that we have room for more.
                            if (THEKOGANS_UTIL_UNLIKELY (tlc.slotCount == TLCThreshold)) {
                                LockGuard<Lock> guard (storage.lock);
                                static constexpr std::size_t flushCount = TLCThreshold / 2;
                                for (std::size_t i = 0; i < flushCount; ++i) {
                                    tlc.Pop ()->Free ();
                                }
                            }
                        }
                        else {
                            LockGuard<Lock> guard (storage.lock);
                            slot->Free ();
                        }
                    }
                }

                static bool FreeSlot (void *ptr) noexcept {
                    if (THEKOGANS_UTIL_LIKELY (ptr != nullptr)) {
                        return Page::Slot::Free (ptr);
                    }
                    return true;
                }

            private:
                inline void AllocPage () noexcept {
                    // Wait until a page is guaranteed to be available or...
                    while (THEKOGANS_UTIL_UNLIKELY (storage.partialPageList == nullptr && storage.pageAllocationInFlight)) {
                        storage.lock.Release ();
                        CPU::YieldSlice ();
                        storage.lock.Acquire ();
                    }
                    // ...we need to allocate it.
                    if (storage.partialPageList == nullptr) {
                        // Set the in flight flag before releasing the lock so that no other thread
                        // tries to allocate a page too.
                        storage.pageAllocationInFlight = true;
                        // Release the lock before dropping down to the OS.
                        // This wont help waiting allocators but if there are
                        // waiting freeers it will alow them to let go of their
                        // slots while we're waiting on the OS.
                        storage.lock.Release ();
                        void *ptr = PageAllocator::Alloc (pageSize);
                        // We're back from the OS land. Reaquire the lock so that we
                        // can wire the freshly minted page in to the list.
                        storage.lock.Acquire ();
                        // NOTE: Between the Release and Acquire above other threads
                        // could have released slots and rewired partial pages back
                        // in to the pool. The reason we don't check and potentially
                        // give back this page is 1. It takes time to check and 2.
                        // even if a page or two are back with one or two empty slots
                        // having a completely empty page is better for cache harvesting.
                        Page *page = new (ptr) Page (*this);
                        // Wire the newly minted page in to the master page list.
                        if constexpr (!IsSingleton) {
                            page->header.masterNext = storage.masterPageList;
                            storage.masterPageList = page;
                        }
                        // Now that a fresh page is available the upstream Alloc will be able
                        // to satisfy harvesting or allocating. Since we hold the lock there's
                        // no chance that this page will be stolen from under us by another thread.
                        // We can now safely clear the in flight flag so that the spinning waiters
                        // drop out and either have a fresh page to harvest/allocate from or permission
                        // to allocate.
                        storage.pageAllocationInFlight = false;
                    }
                }
            };
        } // namespace detail

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorDetail_h)
