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

#if !defined (__thekogans_util_FileAllocator_h)
#define __thekogans_util_FileAllocator_h

#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/File.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Allocator.h"
#include "thekogans/util/BlockAllocator.h"
#include "thekogans/util/Singleton.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocator FileAllocator.h thekogans/util/FileAllocator.h
        ///
        /// \brief
        /// FileAllocator

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocator : public Allocator {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocator)

            /// \brief
            /// Declare \see{DynamicCreatable} boilerplate.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_OVERRIDE (FileAllocator)

            using PtrType = void *;
            static_assert (
                sizeof (PtrType) >= UI64_SIZE,
                "Invalid assumption about FileAllocator::PtrType size.");
            static const std::size_t PtrTypeSize = UI64_SIZE;

            struct _LIB_THEKOGANS_UTIL_DECL Block : public Buffer {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Block)

                PtrType offset;

            protected:
                Block (
                    PtrType offset_,
                    Endianness endianness,
                    std::size_t length,
                    std::size_t readOffset = 0,
                    std::size_t writeOffset = 0,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
                    offset (offset_),
                    Buffer (endianness, length, readOffset, writeOffset, allocator) {}

                friend struct FileAllocator;
            };

            SimpleFile file;
            Allocator::SharedPtr blockAllocator;
            struct Header {
                ui32 blockSize;
                PtrType freeBlock;
                PtrType rootBlock;

                enum {
                    SIZE = UI32_SIZE + PtrTypeSize + PtrTypeSize
                };

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

            FileAllocator (
                const std::string &path,
                std::size_t blockSize = DEFAULT_BLOCK_SIZE,
                std::size_t blocksPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                Allocator::SharedPtr allocator = DefaultAllocator::Instance ());

            inline Endianness GetFileEndianness () const {
                return file.endianness;
            }

            PtrType GetRootBlock ();
            void SetRootBlock (PtrType rootBlock);

            std::size_t Read (
                PtrType offset,
                void *data,
                std::size_t length);
            std::size_t Write (
                PtrType offset,
                const void *data,
                std::size_t length);

            inline Block::SharedPtr CreateBlock (
                    PtrType offset,
                    std::size_t size = 0,
                    bool read = false) {
                Block::SharedPtr block (
                    new Block (
                        offset,
                        GetFileEndianness (),
                        size > 0 ? size : header.blockSize,
                        0,
                        0,
                        blockAllocator));
                if (read) {
                    ReadBlock (block);
                }
                return block;
            }
            inline std::size_t ReadBlock (Block::SharedPtr block) {
                return block->AdvanceWriteOffset (
                    Read (
                        block->offset,
                        block->GetWritePtr (),
                        block->GetDataAvailableForWriting ()));
            }
            inline std::size_t WriteBlock (Block::SharedPtr block) {
                return block->AdvanceReadOffset (
                    Write (
                        block->offset,
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
                /// FileAllocator map type (keyed on path).
                using Map = std::map<std::string, FileAllocator::SharedPtr>;
                /// \brief
                /// FileAllocator map.
                Map map;
                /// \brief
                /// Synchronization lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// Given a block size, return a matching block allocator.
                /// If we don't have one, create it.
                /// \param[in] blockSize Block size.
                /// \return FileAllocator matching the given path.
                FileAllocator::SharedPtr GetFileAllocator (
                    const std::string &path,
                    std::size_t blockSize = DEFAULT_BLOCK_SIZE,
                    std::size_t blocksPerPage = BlockAllocator::DEFAULT_BLOCKS_PER_PAGE,
                    Allocator::SharedPtr allocator = DefaultAllocator::Instance ());
            };

        protected:
            void Save ();

            friend Serializer &operator << (
                Serializer &serializer,
                const Header &header);
            friend Serializer &operator >> (
                Serializer &serializer,
                Header &header);

            /// \brief
            /// FileAllocator is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocator)
        };

        inline Serializer &operator << (
                Serializer &serializer,
                FileAllocator::PtrType ptr) {
            serializer << (ui64)ptr;
            return serializer;
        }

        inline Serializer &operator >> (
                Serializer &serializer,
                FileAllocator::PtrType &ptr) {
            serializer >> (ui64 &)ptr;
            return serializer;
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocator_h)
