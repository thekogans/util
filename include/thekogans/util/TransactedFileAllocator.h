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

/// \struct TransactedFile::Allocator TransactedFileAllocator.h
/// thekogans/util/TransactedFileAllocator.h
///
/// \brief
/// Allocator manages a heap inside a TransactedFile.
struct _LIB_THEKOGANS_UTIL_DECL Allocator : public virtual RefCounted {
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Allocator)

    /// \brief
    /// Allocate a block.
    /// \param[in] size Size of block to allocate.
    /// \return Offset to the allocated block.
    virtual AddressType Alloc (SizeType size) = 0;
    /// \brief
    /// Free a previously Alloc(ated) block.
    /// \param[in] offset Offset of block to free.
    virtual void Free (AddressType offset) = 0;
    /// \brief
    /// Resize a block. Make it bigger or smaller.
    /// \param[in] offset Offset of \see{Block} to resize.
    /// \param[in] newSize New \see{Block} size.
    /// \param[in] moveData true == Copy the data to the new block.
    /// \return If newSize is greater than current size, return the
    /// new \see{Block} offset. If not, return the old \see{Block}
    /// offset.
    virtual AddressType Realloc (
        AddressType offset,
        SizeType newSize,
        bool moveData = true) = 0;
};
