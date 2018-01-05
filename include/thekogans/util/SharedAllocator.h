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

#if !defined (__thekogans_util_SharedAllocator_h)
#define __thekogans_util_SharedAllocator_h

#include <boost/memory_order.hpp>
#include <boost/atomic/detail/config.hpp>
#include <boost/atomic/detail/operations_lockfree.hpp>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/SharedObject.h"

namespace thekogans {
    namespace util {

        /// \struct SharedAllocator SharedAllocator.h thekogans/util/SharedAllocator.h
        ///
        /// \brief
        /// SharedAllocator allocates blocks visible across process boundaries.
        /// The first process is usually the owner, with others being the tenants.
        /// The shared region can be locked in memory (secure == true) to prevent
        /// swapping. Please keep in mind that while SharedAllocator is designed
        /// to be used simultaneously by multiple processes, the processes themselves
        /// cannot deduce what's actually in the heap (that's because there is no
        /// guarantee where in the process's virtual address space the shared region
        /// will be mapped). To be able to use blocks allocated from one process in
        /// another an IPC mechanism is needed to marshal block offsets from one
        /// process to another. This marshaling is outside the scope of the SharedAllocator.
        /// You can begin this process by calling \see{SharedAllocator::GetOffsetFromPtr}
        /// within the process that wants to share a block of memory with a peer.
        /// The offset you get is universal and will work within any process that
        /// has instantiated the same SharedAllocator. Once the process marshals
        /// this offset to it's peer, the peer calls \see{SharedAllocator::GetPtrFromOffset}
        /// to turn that offset in to a local pointer.
        ///
        /// NOTE: When calculating how much space you need for the shared
        /// region, use \see{SharedAllocator::GetAllocatorOverhead} and
        /// \see{SharedAllocator::GetAllocationOverhead} to account for
        /// the overhead needed by the allocator. A simple algorithm to
        /// do that is given in the code snippet below.
        ///
        /// NOTE: To maximize space, SharedAllocator packs allocation requests as
        /// densely as possible without regard to any alignment requirements. If
        /// you need to allocate aligned blocks from SharedAllocator use the
        /// \see{AlignedAllocator} adaptor.
        ///
        /// NOTE: On Windows, if secure == true, you might need to call
        /// SetProcessWorkingSetSize to ensure your process has enough
        /// physical pages.

        struct _LIB_THEKOGANS_UTIL_DECL SharedAllocator : public Allocator {
        protected:
            /// \struct SharedAllocator::Header SharedAllocator.h thekogans/util/SharedAllocator.h
            ///
            /// \brief
            /// Heap header.
            struct Header {
                /// \enum
                /// Size of header.
                enum {
                    SIZE = UI32_SIZE + UI32_SIZE + UI64_SIZE + UI64_SIZE
                };

                /// \brief
                /// A watermark marking this region as a SharedAllocator.
                ui32 magic;
                /// \brief
                /// SpinLock used to serialize access
                /// to the heap from different processes.
                ui32 lock;
                /// \brief
                /// Offset to the head of the free list.
                /// NOTE: The freeList chain is sorted on offset to allow
                /// for easy coalescing of free blocks. This means that
                /// both Alloc and Free run in O (n) time (where n is
                /// the length of the chain). There exists a pathological
                /// Alloc/Free pattern that can make this design decision
                /// perform poorly. It involves freeing every odd or even
                /// block so as to make coalescing impossible. Please keep
                /// that in mind when designing your algorithms.
                ui64 freeList;
                /// \brief
                /// Use this offset to marshal allocations across process boundaries.
                ui64 rootObject;

                /// \brief
                /// ctor.
                /// \param[in] ptr Start of the shared region.
                /// \param[in] size Size of the shared region.
                Header (void *ptr,
                        ui64 size) :
                        magic (MAGIC32),
                        lock (0),
                        freeList (SIZE),
                        rootObject (0) {
                    // Create the first block.
                    new ((ui8 *)ptr + freeList) SharedAllocator::Block (size - SIZE);
                }

                /// \brief
                /// Header is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Header)
            } *header;
            /// \struct SharedAllocator::Lock SharedAllocator.h thekogans/util/SharedAllocator.h
            ///
            /// \brief
            /// Custom spin lock whose storage comes from \see{Header::lock}.
            struct Lock {
                /// \brief
                /// Convenient typedef for boost::atomics::detail::operations<4u, false>.
                typedef boost::atomics::detail::operations<4u, false> operations;
                /// \brief
                /// Convenient typedef for operations::storage_type.
                typedef operations::storage_type storage_type;

                /// \brief
                /// Storage used by this lock.
                storage_type &storage;

                /// \brief
                /// ctor.
                /// \param[in] storage_ Pointer to storage used by this lock.
                explicit Lock (storage_type &storage_) :
                    storage (storage_) {}

                /// \brief
                /// Try to acquire the lock.
                /// \return true = acquired, false = failed to acquire
                inline bool TryAcquire () {
                    return !operations::test_and_set (storage, boost::memory_order_acquire);
                }
                /// \brief
                /// Acquire the lock.
                inline void Acquire () {
                    while (operations::test_and_set (storage, boost::memory_order_acquire)) {
                        // busy-wait
                    }
                }
                /// \brief
                /// Release the lock.
                inline void Release () {
                    operations::clear (storage, boost::memory_order_release);
                }

                /// \brief
                /// Lock is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Lock)
            } lock;
            /// \struct SharedAllocator::Block SharedAllocator.h thekogans/util/SharedAllocator.h
            ///
            /// \brief
            /// Heap block.
            struct Block {
                enum {
                    /// \brief
                    /// Block header size.
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    HEADER_SIZE = UI64_SIZE + UI64_SIZE,
                #else // defined (TOOLCHAIN_CONFIG_Debug)
                    HEADER_SIZE = UI64_SIZE,
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                    /// \brief
                    /// Smallest block size that the SharedAllocator
                    /// can allocate.
                    SMALLEST_BLOCK_SIZE = UI64_SIZE,
                    /// \brief
                    /// Free block size.
                    FREE_BLOCK_SIZE = HEADER_SIZE + SMALLEST_BLOCK_SIZE
                };

            #if defined (TOOLCHAIN_CONFIG_Debug)
                /// \brief
                /// A block watermark. Used in ValidatePtr.
                const ui64 magic;
            #endif // defined (TOOLCHAIN_CONFIG_Debug)
                /// \brief
                /// Block data size.
                ui64 size;
                union {
                    /// \brief
                    /// Pointer to next free block.
                    ui64 next;
                    /// \brief
                    /// User data if block is in use.
                    ui8 data[1];
                };

                /// \brief
                /// ctor.
                /// \param[in] size_ True block size (header + data).
                /// \param[in] next_ Pointer to next free block.
                Block (
                    ui64 size_,
                    ui64 next_ = 0) :
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    magic (MAGIC64),
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                    size (size_ - HEADER_SIZE),
                    next (next_) {}

                /// \brief
                /// Block is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Block)
            };
            /// \brief
            /// This is the smallest valid pointer that SharedAllocator
            /// can return. Since it's constant, we calculate and cache
            /// it in the ctor and use it in ValidatePtr to save two
            /// additions.
            const ui8 *smallestValidPtr;
            /// \brief
            /// Just past the end of the shared region. Since it's constant,
            /// we calculate and cache it in the ctor and use it in ValidatePtr
            /// to save an addition.
            const ui8 *end;

            /// \struct SharedAllocator::Constructor SharedAllocator.h thekogans/util/SharedAllocator.h
            ///
            /// \brief
            struct Constructor : public SharedObject::Constructor {
                /// \brief
                /// Size of the shared region.
                ui64 size;

                /// \brief
                /// ctor.
                /// \param[in] size Size of the shared region.
                explicit Constructor (ui64 size_) :
                    size (size_) {}

                /// \brief
                /// In place construct Header.
                /// \param[in] ptr Where to in place construct the Header.
                /// \return Constructed Header.
                virtual void *operator () (void *ptr) const {
                    return new (ptr) Header (ptr, size);
                }
            };

        public:
            /// \brief
            /// ctor.
            /// \param[in] name Global name used to identify the shared region.
            /// \param[in] size Size of the shared region.
            /// \param[in] secure Lock the pages in memory to prevent swapping.
            SharedAllocator (
                const char *name,
                ui64 size,
                bool secure) :
                header ((Header *)SharedObject::Create (name, size, secure, Constructor (size))),
                lock (header->lock),
                smallestValidPtr ((ui8 *)header + Header::SIZE + Block::HEADER_SIZE),
                end ((ui8 *)header + size) {}
            /// \brief
            /// dtor.
            virtual ~SharedAllocator () {
                SharedObject::Destroy (header);
            }

            /// \brief
            /// Allocate a shared block.
            /// \param[in] size Size of block to allocate.
            /// \return Pointer to the allocated block (0 if out of memory).
            virtual void *Alloc (std::size_t size);
            /// \brief
            /// Free a previously Alloc(ated) block.
            /// \param[in] ptr Pointer to the block returned by Alloc.
            /// \param[in] size Same size parameter previously passed in to Alloc.
            virtual void Free (
                void *ptr,
                std::size_t size);

            /// \brief
            /// Use these three functions to calculate the size of the
            /// shared region needed to accomodate the allocation requests.
            /// NOTE: Due to it's design, the smallest block that a SharedAllocator can
            /// allocate is UI64_SIZE (which should be 8 bytes on all sane architectures).
            /// So, when calculating block sizes make sure to do something simmilar to:
            ///
            /// \code{.cpp}
            /// using namespace thekogans;
            /// static const util::ui64 blockTable[] = {
            ///     // list of block sizes.
            /// };
            /// static const util::ui64 blockTableSize = THEKOGANS_UTIL_ARRAY_SIZE (blockTable);
            /// util::ui64 sharedRegionSize = SharedAllocator::GetAllocatorOverhead ();
            /// for (util::ui64 i = 0; i < blockTableSize; ++i) {
            ///     sharedRegionSize += SharedAllocator::GetAllocationOverhead () +
            ///         std::max (blockTable[i], SharedAllocator::GetSmallestBlockSize ());
            /// }
            /// // sharedRegionSize now contains the size of the shared
            /// // region needed to accomodate the allocation requests.
            /// \endcode

            /// \brief
            /// Return the number of bytes used by the allocator.
            /// \return The number of bytes used by the allocator.
            static ui64 GetAllocatorOverhead () {
                return Header::SIZE;
            }
            /// \brief
            /// Return the number of bytes used by each allocation.
            /// \return The number of bytes used by each allocation.
            static ui64 GetAllocationOverhead () {
                return Block::HEADER_SIZE;
            }
            /// \brief
            /// Return the smallest block size that SharedAllocator can allocate.
            /// \return The smallest block size that SharedAllocator can allocate.
            static ui64 GetSmallestBlockSize () {
                return Block::SMALLEST_BLOCK_SIZE;
            }

            /// \brief
            /// Use this API to convert a local heap pointer to a global block offset.
            /// You can then marshal this offset in to another address space.
            /// \param[in] ptr A local heap pointer.
            /// \return A global block offset.
            inline ui64 GetOffsetFromPtr (void *ptr) const {
                return (ui64)((ui8 *)ptr - (ui8 *)header);
            }
            /// \brief
            /// Use this API to convert a global block offset to a local heap pointer.
            /// \param[in] offset A global block offset.
            /// \return A local heap pointer.
            inline void *GetPtrFromOffset (ui64 offset) const {
                return (ui8 *)header + offset;
            }

            /// \brief
            /// Set the header.rootObject. Use this function to quickly share an
            /// allocation across multiple processes without a lot of marshaling
            /// overhead.
            /// \param[in] rootObject Pointer to root object to set.
            void SetRootObject (void *rootObject);
            /// \brief
            /// Return the header.rootObject.
            /// \return header.rootObject.
            void *GetRootObject ();

        protected:
            /// \brief
            /// Private macros used to impart meaning and reduce code clutter.

            /// \brief
            /// Return a Block * given a block offset.
            /// \param[in] offset Block offset.
            /// \return Block *.
            inline Block *GetBlockFromOffset (ui64 offset) const {
                return offset != 0 ? (Block *)((ui8 *)header + offset) : 0;
            }
            /// \brief
            /// Return a block offset given a Block *.
            /// \param[in] block Block pointer.
            /// \return Block offset.
            inline ui64 GetOffsetFromBlock (Block *block) const {
                return block != 0 ? (ui64)((ui8 *)block - (ui8 *)header) : 0;
            }
            /// \brief
            /// Given a block, calculate the offset of the next block.
            /// \param[in] block Block pointer.
            /// \return Offset of the next block.
            inline Block *GetNextBlock (Block *block) const {
                return (Block *)((ui8 *)block + GetTrueBlockSize (block));
            }
            /// \brief
            /// Given a block, return it's true size.
            /// \param[in] block Block pointer.
            /// \return Block true size.
            inline ui64 GetTrueBlockSize (Block *block) const {
                return Block::HEADER_SIZE + block->size;
            }
            /// \brief
            /// Given a pointer, validate it and return the block it came from.
            /// \param[in] ptr Pointer to validate.
            /// \return If valid, block the pointer belongs to. 0 otherwise.
            inline Block *ValidatePtr (void *ptr) {
                if (ptr >= smallestValidPtr && ptr < end) {
                    Block *block = (Block *)((ui8 *)ptr - Block::HEADER_SIZE);
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    if (block->magic == MAGIC64) {
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                        return block;
                #if defined (TOOLCHAIN_CONFIG_Debug)
                    }
                #endif // defined (TOOLCHAIN_CONFIG_Debug)
                }
                return 0;
            }

            /// \brief
            /// SharedAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (SharedAllocator)
        };

        /// \struct GlobalSharedAllocatorCreateInstance SharedAllocator.h thekogans/util/SharedAllocator.h
        ///
        /// \brief
        /// Call GlobalSharedAllocatorCreateInstance::Parameterize before the first call to
        /// GlobalSharedAllocator::Instance () to provice custom ctor arguments to the global
        /// shared allocator instance.

        struct _LIB_THEKOGANS_UTIL_DECL GlobalSharedAllocatorCreateInstance {
        private:
            /// \brief
            /// Global name used to identify the shared region.
            static const char *name;
            /// \brief
            /// Size of the shared region.
            static ui64 size;
            /// \brief
            /// Lock the pages in memory to prevent swapping.
            static bool secure;

        public:
            /// \brief
            /// Default GlobalSharedAllocator name.
            static const char *DEFAULT_GLOBAL_SHARED_ALLOCATOR_NAME;
            /// \brief
            /// Default GlobalSharedAllocator size.
            enum : ui64 {
                DEFAULT_GLOBAL_SHARED_ALLOCATOR_SIZE = 16 * 1024
            };
            /// \brief
            /// Call before the first use of GlobalSharedAllocator::Instance.
            /// \param[in] name_ Global name used to identify the shared region.
            /// \param[in] size_ Size of the shared region.
            /// \param[in] secure_ Lock the pages in memory to prevent swapping.
            static void Parameterize (
                const char *name_ = DEFAULT_GLOBAL_SHARED_ALLOCATOR_NAME,
                ui64 size_ = DEFAULT_GLOBAL_SHARED_ALLOCATOR_SIZE,
                bool secure_ = false);

            /// \brief
            /// Create a global shared allocator with custom ctor arguments.
            /// \return A global shared allocator with custom ctor arguments.
            SharedAllocator *operator () ();
        };

        /// \struct GlobalSharedAllocator SharedAllocator.h thekogans/util/SharedAllocator.h
        ///
        /// \brief
        /// The one and only global shared allocator instance.
        struct _LIB_THEKOGANS_UTIL_DECL GlobalSharedAllocator :
            public Singleton<SharedAllocator, SpinLock, GlobalSharedAllocatorCreateInstance> {};

        /// \def THEKOGANS_UTIL_IMPLEMENT_SHARED_ALLOCATOR_FUNCTIONS(type)
        /// Macro to implement SharedAllocator functions.
        #define THEKOGANS_UTIL_IMPLEMENT_SHARED_ALLOCATOR_FUNCTIONS(type)\
        void *type::operator new (std::size_t size) {\
            assert (size == sizeof (type));\
            return thekogans::util::GlobalSharedAllocator::Instance ().Alloc (size);\
        }\
        void *type::operator new (\
                std::size_t size,\
                std::nothrow_t) throw () {\
            assert (size == sizeof (type));\
            return thekogans::util::GlobalSharedAllocator::Instance ().Alloc (size);\
        }\
        void *type::operator new (\
                std::size_t size,\
                void *ptr) {\
            assert (size == sizeof (type));\
            return ptr;\
        }\
        void type::operator delete (void *ptr) {\
            thekogans::util::GlobalSharedAllocator::Instance ().Free (ptr, sizeof (type));\
        }\
        void type::operator delete (\
                void *ptr,\
                std::nothrow_t) throw () {\
            thekogans::util::GlobalSharedAllocator::Instance ().Free (ptr, sizeof (type));\
        }\
        void type::operator delete (\
            void *,\
            void *) {}

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_SharedAllocator_h)
