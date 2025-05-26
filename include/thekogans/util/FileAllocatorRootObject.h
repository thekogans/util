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

#if !defined (__thekogans_util_FileAllocatorRootObject_h)
#define __thekogans_util_FileAllocatorRootObject_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/FileAllocatorObject.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorRootObject FileAllocatorRootObject.h
        /// thekogans/util/FileAllocatorRootObject.h
        ///
        /// \brief

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorRootObject :
                public FileAllocatorObject,
                public Subscriber<FileAllocatorObjectEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocatorRootObject)

            /// \brief
            /// ctor.
            FileAllocatorRootObject (FileAllocator::SharedPtr fileAllocator) :
                FileAllocatorObject (fileAllocator, fileAllocator->header.rootOffset) {}

        protected:
            // BufferedFileTransactionParticipant
            /// \brief
            /// Flush the internal cache to file.
            virtual void Flush () override {}

            /// \brief
            /// Reload from file.
            virtual void Reload () override {}

            // FileAllocatorObjectEvents
            /// \brief
            /// We've just updated the offset.
            /// \param[in] self \see{FileAllocatorObject} that just updated the offset (this).
            virtual void OnFileAllocatorObjectOffsetChanged (
                    FileAllocatorObject::SharedPtr self) noexcept override {
                BufferedFile::SharedPtr file = fileAllocator->GetFile ();
                file->Seek (0, SEEK_SET);
                *file << MAGIC32 << fileAllocator->header;
            }

            /// \brief
            /// FileAllocatorRootObject is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorRootObject)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorRootObject_h)
