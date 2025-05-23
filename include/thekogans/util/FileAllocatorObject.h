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
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BufferedFileTransactionParticipant.h"

namespace thekogans {
    namespace util {

        /// \struct FileAllocatorObject FileAllocatorObject.h thekogans/util/FileAllocatorObject.h
        ///
        /// \brief
        /// A FileAllocatorObject is an object that lives in a \see{FileAllocator} and
        /// participates in \see{BufferedFile::Transaction}.

        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorObject :
                public Subscriber<FileAllocatorEvents>,
                public BufferedFileTransactionParticipant {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (FileAllocatorObject)

        protected:
            /// \brief
            /// \see{FileAllocator} where this object resides.
            FileAllocator &fileAllocator;
            /// \brief
            /// Our address within the \see{FileAllocator}.
            FileAllocator::PtrType offset;

        public:
            /// \brief
            /// ctor.
            /// \param[in] fileAllocator \see{FileAllocator} where this object resides.
            /// \param[in] transaction Optional transaction to join.
            /// NOTE: This parameter is used by temporary \see{BufferedFileTransactionParticipant}
            /// to add themselves to the participants list.
            FileAllocatorObject (
                FileAllocator &fileAllocator_,
                FileAllocator::PtrType offset_,
                BufferedFile::Transaction::SharedPtr transaction = nullptr);
            /// \brief
            /// dtor.
            virtual ~FileAllocatorObject () {}

            /// \brief
            /// Return the offset.
            /// \return Offset.
            inline FileAllocator::PtrType GetOffset () const {
                return offset;
            }

        private:
            // FileAllocatorEvents
            /// \brief
            /// \see{FileAllocator} just created a new \see{BufferedFile::Transaction}.
            /// Subscribe to it's events.
            /// \param[in] fileAllocator \see{FileAllocator} that created the transaction.
            /// \param[in] transaction \see{BufferedFile::Transaction} that was created.
            virtual void OnFileAllocatorCreateTransaction (
                    FileAllocator::SharedPtr /*fileAllocator*/,
                    BufferedFile::Transaction::SharedPtr transaction) noexcept override {
                BufferedFileTransactionParticipant::Subscribe (*transaction);
            }

            /// \brief
            /// FileAllocatorObject is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorObject)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorObject_h)
