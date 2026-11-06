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

/// \struct Dictionary TransactedFileDictionary.h thekogans/util/TransactedFileDictionary.h
///
/// \brief
/// Dictionary provides a global map from type string to type id and
/// vise versa.
struct _LIB_THEKOGANS_UTIL_DECL Dictionary : public virtual Object {
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Dictionary)

    using TypeIdType = ui32;
    static constexpr std::size_t TypeIdTypeSize = Width<TypeIdType>::value;

    virtual TypeIdType GetOrCreateTypeId (const std::string &type) = 0;
    virtual std::string ResolveTypeId (TypeIdType typeId) = 0;
};

struct SystemDictionary {
    static constexpr Dictionary::TypeIdType None = 0;
    static constexpr Dictionary::TypeIdType MaxReservedTypeId = 1024;
    static constexpr Dictionary::TypeIdType FirstDictionaryTypeId = MaxReservedTypeId + 1;

    static std::string ResolveTypeId (Dictionary::TypeIdType typeId) noexcept;
    static Dictionary::TypeIdType GetReservedTypeId (const std::string &type) noexcept;
};
