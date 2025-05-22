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

#if !defined (__thekogans_util_BufferedFileTransactionParticipant_h)
#define __thekogans_util_BufferedFileTransactionParticipant_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/BufferedFile.h"

namespace thekogans {
    namespace util {

        /// \struct BufferedFileTransactionParticipant BufferedFileTransactionParticipant.h
        /// thekogans/util/BufferedFileTransactionParticipant.h
        ///
        /// \brief

        struct _LIB_THEKOGANS_UTIL_DECL BufferedFileTransactionParticipant :
                public Subscriber<BufferedFile::TransactionEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BufferedFileTransactionParticipant)

            BufferedFileTransactionParticipant (
                BufferedFile::Transaction::SharedPtr transaction = nullptr);
            virtual ~BufferedFileTransactionParticipant () {}

        protected:
            /// \brief
            /// Flush the internal cache (if any) to file.
            virtual void Flush () = 0;

            /// \brief
            /// Reload from file.
            virtual void Reload () = 0;

            // BufferedFile::TransactionEvents
            virtual void OnTransactionBegin (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Flush ();
            }
            virtual void OnTransactionCommit (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Flush ();
            }
            virtual void OnTransactionAbort (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Reload ();
            }

            /// \brief
            /// FileAllocatorObject is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BufferedFileTransactionParticipant)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BufferedFileTransactionParticipant_h)
