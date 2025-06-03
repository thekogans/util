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

        /// \brief
        /// Forward declaration of \see{FileAllocatorObject}
        /// needed by \see{FileAllocatorObjectEvents}.
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
        /// participates in \see{BufferedFile::TransactionEvents}.
        struct _LIB_THEKOGANS_UTIL_DECL FileAllocatorObject :
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
            /// Our address inside the \see{FileAllocator}.
            FileAllocator::PtrType offset;

        public:
            /// \brief
            /// ctor.
            /// \param[in] fileAllocator \see{FileAllocator} where this object resides.
            /// \param[in] offset Offset of the \see{FileAllocator::BlockInfo}.
            FileAllocatorObject (
                FileAllocator::SharedPtr fileAllocator_,
                FileAllocator::PtrType offset_) :
                BufferedFileTransactionParticipant (fileAllocator_->GetFile ()),
                fileAllocator (fileAllocator_),
                offset (offset_) {}
            /// \brief
            /// dtor.
            virtual ~FileAllocatorObject () {}


            /// \struct FileAllocatorObject::OffsetTracker FileAllocatorObject.h
            /// thekogans/util/FileAllocatorObject.h
            ///
            /// \brief
            /// Track offset changes and update an external offset reference.
            struct OffsetTracker : public Subscriber<FileAllocatorObjectEvents> {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (OffsetTracker)

            private:
                /// \brief
                /// Reference to offset we're tracking.
                FileAllocator::PtrType &offset;

            public:
                /// \brief
                /// \param[out] offset_ Reference to offset we're tracking.
                /// \param[in] object Object to listen to for offset updates.
                OffsetTracker (
                        FileAllocator::PtrType &offset_,
                        FileAllocatorObject &object) :
                        offset (offset_) {
                    Subscriber<FileAllocatorObjectEvents>::Subscribe (object);
                }

            protected:
                // FileAllocatorObjectEvents
                /// \brief
                /// We've just updated the offset.
                /// \param[in] fileAllocatorObject \see{FileAllocatorObject}
                /// that just updated the offset.
                virtual void OnFileAllocatorObjectOffsetChanged (
                        FileAllocatorObject::SharedPtr fileAllocatorObject) noexcept override {
                    offset = fileAllocatorObject->GetOffset ();
                }

                /// \brief
                /// OffsetTracker is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (OffsetTracker)
            };

            /// \brief
            /// Return the fileAllocator.
            /// \return fileAllocator.
            inline FileAllocator::SharedPtr GetFileAllocator () const {
                return fileAllocator;
            }
            /// \brief
            /// Return the offset.
            /// \return offset.
            inline FileAllocator::PtrType GetOffset () const {
                return offset;
            }

            /// \brief
            /// Return the size of the object on disk.
            /// \return Size of the object on disk.
            virtual std::size_t Size () const = 0;

            /// \brief
            /// Delete the disk image and reset the internal state.
            virtual void Reset () = 0;

        private:
            // BufferedFileTransactionParticipant
            /// \brief
            /// If needed allocate space from \see{BufferedFile}.
            virtual void Allocate () override;

            /// \brief
            /// FileAllocatorObject is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileAllocatorObject)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileAllocatorObject_h)
