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

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        // 1. Forward declare the master template struct first
        template <typename T, typename... Policies>
        struct SlabAllocator;

        template<typename T>
        struct DefaultInstanceCreator;

        namespace Policy {
            // Value policies use a standard template wrapper
            template <std::size_t Value> struct TLCThreshold { static constexpr std::size_t value = Value; };
            template <std::size_t Value> struct SlotsPerPage { static constexpr std::size_t value = Value; };
            template <std::size_t Value> struct CacheLineSize { static constexpr std::size_t value = Value; };
            template <std::size_t Value> struct Id             { static constexpr std::size_t value = Value; };

            // Type policies hold the raw type definition
            template <typename T> struct Lock { using type = T; };
            template <typename T> struct PageAllocator { using type = T; };
            template <template <typename> typename T>
            struct InstanceCreator {
                template <typename U> using template_type = T<U>;
            };

            namespace Compaction {
                struct Disable { static constexpr bool value = false; };
                struct Enable  { static constexpr bool value = true; };
            }
        }

        namespace detail {
            /// \brief
            /// Default slots per page. A tuning knob meant to limit page
            /// allocations for a heavily allocated type.
            static constexpr std::size_t DEFAULT_SLOTS_PER_PAGE = 512;
            /// \brief
            /// Default Thread Local Cache (TLC) size.
            /// A tuning knob meant to limit lock contention on a heavily contested type.
            static constexpr std::size_t DEFAULT_TLC_THRESHOLD = 128;
            /// \brief
            /// Default cache line size.
            /// VERY IMPORTANT: This is a very critical parameter. On modern megacore
            /// architectures (M5) cache eviction is perhaps the single biggest drain
            /// on application performance. This is why it's exposed as a template parameter.
            /// If, for some reason, the compiler can't get it right, you have the power to
            /// force it in to a particular value. It goes without saying that this value
            /// must be a power of 2.
            static constexpr std::size_t DEFAULT_CACHE_LINE_SIZE = SYSTEM_CACHE_LINE_SIZE;

            // --- VALUE EXTRACTOR MECHANICS ---
            template <template <std::size_t> typename TargetPolicy, std::size_t DefaultValue, typename... Policies>
            struct GetValuePolicy {
                static constexpr std::size_t value = DefaultValue; // Base case: fallback to default
            };

            template <template <std::size_t> typename TargetPolicy, std::size_t DefaultValue, std::size_t CurrentValue, typename... Rest>
            struct GetValuePolicy<TargetPolicy, DefaultValue, TargetPolicy<CurrentValue>, Rest...> {
                static constexpr std::size_t value = CurrentValue; // Found it: extract the template value
            };

            template <template <std::size_t> typename TargetPolicy, std::size_t DefaultValue, typename Head, typename... Rest>
            struct GetValuePolicy<TargetPolicy, DefaultValue, Head, Rest...> {
                static constexpr std::size_t value = GetValuePolicy<TargetPolicy, DefaultValue, Rest...>::value; // Skip and keep looking
            };

            // --- TYPE EXTRACTOR MECHANICS ---
            template <template <typename> typename TargetPolicy, typename DefaultType, typename... Policies>
            struct GetTypePolicy { using type = DefaultType; };

            template <template <typename> typename TargetPolicy, typename DefaultType, typename CurrentType, typename... Rest>
            struct GetTypePolicy<TargetPolicy, DefaultType, TargetPolicy<CurrentType>, Rest...> { using type = CurrentType; };

            template <template <typename> typename TargetPolicy, typename DefaultType, typename Head, typename... Rest>
            struct GetTypePolicy<TargetPolicy, DefaultType, Head, Rest...> {
                using type = typename GetTypePolicy<TargetPolicy, DefaultType, Rest...>::type;
            };

            // Base Case: Fallback to the engine's default allocator creator if the tag is missing
            template <typename T, typename DefaultType, typename... Policies>
            struct GetInstanceCreatorPolicyHelper { using type = DefaultType; };

            // Match Case: If we encounter the InstanceCreator type tag wrapper, extract its template type!
            template <typename T, typename DefaultType, template <typename> typename CurrentTemplate, typename... Rest>
            struct GetInstanceCreatorPolicyHelper<T, DefaultType, Policy::InstanceCreator<CurrentTemplate>, Rest...> {
                using type = typename Policy::InstanceCreator<CurrentTemplate>::template template_type<T>;
            };

            // Traversal Case: Skip unrelated tags and keep looking down the pack
            template <typename T, typename DefaultType, typename Head, typename... Rest>
            struct GetInstanceCreatorPolicyHelper<T, DefaultType, Head, Rest...> {
                using type = typename GetInstanceCreatorPolicyHelper<T, DefaultType, Rest...>::type;
            };

            // Publicly exposed extractor helper mapping for your main inheritance block
            template <typename T, typename... Policies>
            struct GetInstanceCreatorPolicy {
                using type = typename GetInstanceCreatorPolicyHelper<
                    SlabAllocator<T, Policies...>,
                    DefaultInstanceCreator<SlabAllocator<T, Policies...>>,
                    Policies...>::type;
            };

            // Extractor trait for boolean compaction policy
            template <typename DefaultType, typename... Policies>
            struct GetCompactionStrategy { using type = DefaultType; };

            template <typename DefaultType, typename... Rest>
            struct GetCompactionStrategy<DefaultType, Policy::Compaction::Enable, Rest...> {
                using type = Policy::Compaction::Enable;
            };

            template <typename DefaultType, typename... Rest>
            struct GetCompactionStrategy<DefaultType, Policy::Compaction::Disable, Rest...> {
                using type = Policy::Compaction::Disable;
            };

            // A compile-time trait to check if a type is a valid policy for our allocator
            template <typename P> struct IsValidSlabPolicy : std::false_type {};

            // Explicitly whitelist every supported tag right here in the detail namespace!
            template <std::size_t V> struct IsValidSlabPolicy<Policy::SlotsPerPage<V>> : std::true_type {};
            template <std::size_t V> struct IsValidSlabPolicy<Policy::TLCThreshold<V>> : std::true_type {};
            template <std::size_t V> struct IsValidSlabPolicy<Policy::CacheLineSize<V>> : std::true_type {};
            template <std::size_t V> struct IsValidSlabPolicy<Policy::Id<V>>            : std::true_type {};
            template <typename L>    struct IsValidSlabPolicy<Policy::Lock<L>>          : std::true_type {};
            template <typename A>    struct IsValidSlabPolicy<Policy::PageAllocator<A>> : std::true_type {};
            template <template <typename> typename C> struct IsValidSlabPolicy<Policy::InstanceCreator<C>> : std::true_type {};
            template <> struct IsValidSlabPolicy<Policy::Compaction::Enable>  : std::true_type {};
            template <> struct IsValidSlabPolicy<Policy::Compaction::Disable> : std::true_type {};

            // Helper to evaluate the entire variadic pack at once
            template <typename... Policies>
            constexpr bool ValidateSlabPolicies() {
                return (IsValidSlabPolicy<Policies>::value && ...);
            }

            /// \struct DefaultPageAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
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

            /// \struct StdPageAllocator SlabAllocator.h thekogans/util/SlabAllocator.h
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
        } // namespace detail

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SlabAllocatorDetail_h)
