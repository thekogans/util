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
        /// BufferedFileTransactionParticipants are objects that listen to
        /// \see{BufferedFile::TransactionEvents} and are able to flush and reload
        /// themselves to and from a \see{BufferedFile}.

        struct _LIB_THEKOGANS_UTIL_DECL BufferedFileTransactionParticipant :
                public Subscriber<BufferedFile::TransactionEvents> {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (BufferedFileTransactionParticipant)

            /// \brief
            /// ctor.
            /// \param[in] transaction If this parameter is not null
            /// that means we're a temporary transaction participant.
            BufferedFileTransactionParticipant (
                BufferedFile::Transaction::SharedPtr transaction = nullptr);
            /// \brief
            /// dtor.
            virtual ~BufferedFileTransactionParticipant () {}

        protected:
            /// \brief
            /// Flush the internal cache to file.
            virtual void Flush () = 0;

            /// \brief
            /// Reload from file.
            virtual void Reload () = 0;

            // BufferedFile::TransactionEvents
            /// \brief
            /// Transaction is beginning. Flush internal cache to file.
            /// \param[in] transaction \see{BufferedFile::Transaction} that's beginning.
            virtual void OnTransactionBegin (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Flush ();
            }
            /// \brief
            /// Transaction is commiting. Flush internal cache to file.
            /// \param[in] transaction \see{BufferedFile::Transaction} that's commiting.
            virtual void OnTransactionCommit (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Flush ();
            }
            /// \brief
            /// Transaction is aborting. Reload from file.
            /// \param[in] transaction \see{BufferedFile::Transaction} that's aborting.
            virtual void OnTransactionAbort (
                    BufferedFile::Transaction::SharedPtr /*transaction*/) noexcept override {
                Reload ();
            }

            /// \brief
            /// BufferedFileTransactionParticipant is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (BufferedFileTransactionParticipant)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_BufferedFileTransactionParticipant_h)
