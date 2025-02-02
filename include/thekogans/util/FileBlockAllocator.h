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

#if !defined (__thekogans_util_FileBlockAllocator_h)
#define __thekogans_util_FileBlockAllocator_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/File.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct FileBlockAllocator FileBlockAllocator.h thekogans/util/FileBlockAllocator.h
        ///
        /// \brief
        /// FileBlockAllocator

        struct _LIB_THEKOGANS_UTIL_DECL FileBlockAllocator : public Allocator {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileBlockAllocator)

            /// \brief
            /// Declare \see{DynamicCreatable} boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (FileBlockAllocator)

            using PtrType = void *;
            static_assert (
                sizeof (PtrType) >= UI64_SIZE,
                "Invalid assumption about FileBlockAllocator::PtrType size.");
            static const std::size_t PtrTypeSize = UI64_SIZE;

        private:
            SimpleFile file;
            Allocator::SharedPtr blockAllocator;
            struct Header {
                ui32 blockSize;
                PtrType freeBlock;
                PtrType rootBlock;

                Header (ui32 blockSize_) :
                    blockSize (blockSize_ >= PtrTypeSize ? blockSize_ : PtrTypeSize),
                    freeBlock (nullptr),
                    rootBlock (nullptr) {}
            } header;
            SpinLock spinLock;

        public:
            enum {
                DEFAULT_BLOCK_SIZE = 512
            };

            FileBlockAllocator (
                const std::string &path,
                std::size_t blockSize = DEFAULT_BLOCK_SIZE,
                std::size_t blocksPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            inline Endianness GetFileEndianness () const {
                return file.endianness;
            }

            PtrType GetRootBlock ();
            void SetRootBlock (PtrType rootBlock);

            virtual PtrType Alloc (std::size_t size) override;
            virtual void Free (
                PtrType offset,
                std::size_t size) override;

            std::size_t Read (
                PtrType offset,
                void *data,
                std::size_t length);
            Buffer::SharedPtr ReadBlock (PtrType offset);
            inline Buffer::SharedPtr CreateBlock () const {
                return new Buffer (
                    GetFileEndianness (), header.blockSize, 0, 0, blockAllocator);
            }
            std::size_t Write (
                PtrType offset,
                const void *data,
                std::size_t length);
            inline std::size_t WriteBlock (
                    PtrType offset,
                    Buffer::SharedPtr block) {
                return block->AdvanceReadOffset (
                    Write (
                        offset,
                        block->GetReadPtr (),
                        block->GetDataAvailableForReading ()));
            }

            /// \struct BlockAllocator::Pool BlockAllocator.h thekogans/util/BlockAllocator.h
            ///
            /// \brief
            /// Use Pool to recycle and reuse block allocators.
            struct _LIB_THEKOGANS_UTIL_DECL Pool : public Singleton<Pool> {
            private:
                /// \brief
                /// FileBlockAllocator map type (keyed on path).
                using Map = std::map<std::string, FileBlockAllocator::SharedPtr>;
                /// \brief
                /// FileBlockAllocator map.
                Map map;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// Given a block size, return a matching block allocator.
                /// If we don't have one, create it.
                /// \param[in] blockSize Block size.
                /// \return FileBlockAllocator matching the given path.
                FileBlockAllocator::SharedPtr GetFileBlockAllocator (
                    const std::string &path,
                    std::size_t blockSize = DEFAULT_BLOCK_SIZE,
                    std::size_t blocksPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            };

        private:
            void Save ();

            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            /// \brief
            /// FileBlockAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileBlockAllocator)
        };

        inline Serializer &operator << (
                Serializer &serializer,
                FileBlockAllocator::PtrType ptr) {
            serializer << (ui64)ptr;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileBlockAllocator::PtrType &ptr) {
            serializer >> (ui64 &)ptr;
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileBlockAllocator_h)
