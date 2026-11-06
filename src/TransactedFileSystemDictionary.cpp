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

#include "thekogans/util/Types.h"
#include "thekogans/util/TransactedFileHashDictionary.h"
#include "thekogans/util/TransactedFile.h"

namespace thekogans {
    namespace util {

        static constexpr TransactedFile::Dictionary::TypeIdType HashDictionary = 1;

        std::string TransactedFile::SystemDictionary::ResolveTypeId (Dictionary::TypeIdType typeId) noexcept {
            switch (typeId) {
                case HashDictionary: {
                    return TransactedFileHashDictionary::TYPE;
                }
                default: {
                    return std::string ();
                }
            }
        }

        TransactedFile::Dictionary::TypeIdType TransactedFile::SystemDictionary::GetReservedTypeId (
                const std::string &type) noexcept {
            if (type == TransactedFileHashDictionary::TYPE) {
                return HashDictionary;
            }
            return None;
        }

    } // namespace util
} // namespace thekogans
