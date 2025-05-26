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

#if !defined (__thekogans_util_FileAllocatorObject_h)
#define __thekogans_util_FileAllocatorObject_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BufferedFile.h"
#include "thekogans/util/BufferedFileTransactionParticipant.h"

namespace thekogans {
    namespace util {

        struct FileAllocatorObject;

        /// \struct FileAllocatorObjectEvents FileAllocatorObject.h
        /// thekogans/util/FileAllocatorObject.h
        ///
        /// \brief
        /// Subscribe to FileAllocatorObjectEvents to receive offset change notifications.

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorObjectEvents {
            /// \brief
            /// dtor.
            virtual ~FileAllocatorObjectEvents () {}

            /// \brief
            /// \see{FileAllocatorObject} offset changed. Update whatever state you need.
            /// \param[in] file \see{FileAllocatorObject} whose offset changed.
            virtual void OnFileAllocatorObjectOffsetChanged (
                RefCounted::SharedPtr<FileAllocatorObject> /*fileAllocatorObject*/) noexcept {}
        };

        /// \struct FileAllocatorObject FileAllocatorObject.h thekogans/util/FileAllocatorObject.h
        ///
        /// \brief
        /// A FileAllocatorObject is an object that lives in a \see{FileAllocator} and
        /// participates in \see{BufferedFile::Transaction}.

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorObject :
                public Subscriber<BufferedFileEvents>,
                public BufferedFileTransactionParticipant,
                public Producer<FileAllocatorObjectEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocatorObject)

        protected:
            /// \brief
            /// \see{FileAllocator} where this object resides.
            FileAllocator::SharedPtr fileAllocator;
            /// \brief
            /// Our address within the \see{FileAllocator}.
            FileAllocator::PtrType &offset;

        public:
            /// \brief
            /// ctor.
            /// \param[in] fileAllocator \see{FileAllocator} where this object resides.
            /// \param[in] offset Offset of the \see{FileAllocator::BlockInfo}.
            /// \param[in] transaction Optional transaction to join.
            /// NOTE: This parameter is used by temporary \see{BufferedFileTransactionParticipant}
            /// to add themselves to the participants list.
            FileAllocatorObject (
                FileAllocator::SharedPtr fileAllocator_,
                FileAllocator::PtrType &offset_);
            /// \brief
            /// dtor.
            virtual ~FileAllocatorObject () {}

            inline FileAllocator::SharedPtr GetFileAllocator () const {
                return fileAllocator;
            }
            /// \brief
            /// Return the offset.
            /// \return Offset.
            inline FileAllocator::PtrType GetOffset () const {
                return offset;
            }

        private:
            // BufferedFileEvents
            /// \brief
            /// \see{BufferedFile} just created a new \see{BufferedFile::Transaction}.
            /// Subscribe to it's events.
            /// \param[in] file \see{BufferedFile} that created the transaction.
            virtual void OnBufferedFileCreateTransaction (
                    BufferedFile::SharedPtr file) noexcept override {
                BufferedFileTransactionParticipant::Subscribe (*file->GetTransaction ());
            }

            /// \brief
            /// FileAllocatorObject is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorObject)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorObject_h)
