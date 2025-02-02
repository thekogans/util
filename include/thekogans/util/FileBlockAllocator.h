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

namespace thekogans {
    namespace util {

        /// \struct FileBlockAllocator FileBlockAllocator.h thekogans/util/FileBlockAllocator.h
        ///
        /// \brief
        /// FileBlockAllocator

        struct _LIB_THEKOGANS_UTIL_DECL FileBlockAllocator {
        private:
            SimpleFile file;
            struct Header {
                ui32 blockSize;
                ui64 freeBlock;
                ui64 rootBlock;

                Header (ui32 blockSize_) :
                    blockSize (blockSize_ >= UI64_SIZE ? blockSize_ : UI64_SIZE),
                    freeBlock (NIDX64),
                    rootBlock (NIDX64) {}
            } header;
            SpinLock spinLock;

        public:
            FileBlockAllocator (
                const std::string &path,
                ui32 blockSize);

            ui64 GetRootBlock ();
            void SetRootBlock (ui64 rootBlock);

            ui64 Alloc ();
            void Free (ui64 offset);

            void Read (
                ui64 offset,
                void *data);
            void Write (
                ui64 offset,
                const void *data);

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

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileBlockAllocator_h)
