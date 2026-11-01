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

/// \struct Registry TransactedFileRegistry.h thekogans/util/TransactedFileRegistry.h
///
/// \brief
/// Registry provides global associative storage for \see{TransactedFile}
/// clients. Use it to store and retrieve any value derived from \see{Object}.
/// The key type is std::string.
struct _LIB_THEKOGANS_UTIL_DECL Registry : public virtual Object {
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Registry)

    /// \brief
    /// Given a key, retrieve the associated value. If key is not found,
    /// return nullptr.
    /// \param[in] key Key whose value to retrieve.
    /// \return Value @ key, nullptr if key is not found.
    virtual Object::SharedPtr GetValue (const std::string & /*key*/) = 0;
    virtual bool InsertValue (
        const std::string & /*key*/,
        TransactedFile::Object::SharedPtr /*value*/) = 0;
    virtual void RemoveValue (const std::string & /*key*/) = 0;
};
