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

/// \struct TransactedFile::Header TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// Header contains the global heap values.
struct _LIB_THEKOGANS_UTIL_DECL Header :
        public Object,
        public Subscriber<ObjectEvents> {
    /// \brief
    /// Declare \see{RefCounted} pointers.
    THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (Header)

    static constexpr AddressType BLOCK_OFFSET = Block::BLOCK_0_OFFSET;

    /// \brief
    /// If set, zero out freed blocks.
    static constexpr ui32 FLAGS_SECURE = 1;

private:
    /// \brief
    /// Heap flags.
    Flags32 flags{0};
    AddressType dictionaryOffset{0};
    AddressType allocatorOffset{0};
    /// \brief
    /// Contains the offset of the \see{Registry}.
    AddressType registryOffset{0};
    SpinLock spinLock;

public:
    /// \brief
    /// The size of the header on disk.
    static constexpr std::size_t SIZE =
        UI32_SIZE +       // magic
        UI32_SIZE +       // flags
        AddressTypeSize + // dictionaryOffset
        AddressTypeSize + // allocatorOffset
        AddressTypeSize;  // regisryOffset

    Header (
        TransactedFile::SharedPtr file = nullptr,
        TransactedFile::AddressType offset = 0,
        ui32 flags_ = 0,
        DynamicCreatable::TypeDictionary::SharedPtr dictionary_ = nullptr,
        Allocator::SharedPtr allocator_ = nullptr,
        Registry::SharedPtr registry_ = nullptr);

    /// \brief
    /// Return true if secure.
    /// \return true == secure.
    inline bool IsSecure () const {
        return flags.Test (FLAGS_SECURE);
    }

private:
    /// \brief
    /// Read the types dictionary from the serializer.
    /// \param[in] serializer \see{Serializer} to read the types from.
    virtual void Read (Serializer &serializer) override;
    /// \brief
    /// Write the types dictionary to the serializer.
    /// \param[in] serializer \see{Serializer} to write the types to.
    virtual void Write (Serializer &serializer) override;

    virtual void OnTransactedFileObjectAllocBlock (Object::SharedPtr /*object*/) noexcept override {
        SetDirty ();
    }

    /// \brief
    /// TransactedFileTypeDictionary is neither copy constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Header)
};
