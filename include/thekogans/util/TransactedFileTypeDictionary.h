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

#if !defined (__thekogans_util_TransactedFileTypeDictionary_h)
#define __thekogans_util_TransactedFileTypeDictionary_h

#include <string>
#include <vector>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        /// \struct TransactedFileTypeDictionary TransactedFileTypeDictionary.h
        /// thekogans/util/TransactedFileTypeDictionary.h
        ///
        /// \brief
        /// TransactedFileTypeDictionary is a \see{TransactedFile::Regitry}.
        /// It uses a \see{TransactedFileBree} to provide global ordered,
        /// associative storage for \see{TransactedFile} clients. Use it to
        /// store and retrieve practically any value derived from \see{Serializable}.
        /// The key type is std::string. TransactedFileTypeDictionary is thread safe.
        struct _LIB_THEKOGANS_UTIL_DECL TransactedFileTypeDictionary :
                public DynamicCreatable::DefaultTypeDictionary,
                public TransactedFile::Object {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (TransactedFileTypeDictionary)

        private:
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            virtual IdType GetOrCreateTypeId (const char *type) override;
            virtual const char *ResolveTypeId (IdType id) override;

        protected:
            // Serializable
            virtual std::size_t Size (Serializer &serializer) noexcept override;
            /// \brief
            /// Read the types dictionary from the serializer.
            /// \param[in] serializer \see{Serializer} to read the types from.
            virtual void Read (Serializer &serializer) override;
            /// \brief
            /// Write the types dictionary to the serializer.
            /// \param[in] serializer \see{Serializer} to write the types to.
            virtual void Write (Serializer &serializer) override;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_TransactedFileTypeDictionary_h)
