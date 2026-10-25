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
/// Allocator manages a heap on permanent storage.
struct _LIB_THEKOGANS_UTIL_DECL Allocator : public virtual Object {
    /// \brief
    /// Allocator is a \see{util::DynamicCreatable} abstract base.
    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Allocator)

#if defined (THEKOGANS_UTIL_TYPE_Static)
    /// \brief
    /// Register all known derivatives. This method is meant to be added
    /// to as new Allocator derivatives are added to the system.
    static void StaticInit ();
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

protected:
    /// \struct TransactedFile::Allocator::Header TransactedFileAllocator.h
    /// thekogans/util/TransactedFileAllocator.h
    ///
    /// \brief
    /// Header contains the global heap vailues.
    struct _LIB_THEKOGANS_UTIL_DECL Header {
        /// \brief
        /// When creating a new heap, this flag will stamp the structure
        /// of \see{Block} so that if you ever try to open it with a
        /// wrongly compiled version of thekogans_util it will complain
        /// instead of corrupting your data.
        static const ui32 FLAGS_BLOCK_USES_MAGIC = 1;
        /// \brief
        /// If set, zero out freed blocks.
        static const ui32 FLAGS_SECURE = 2;
        /// \brief
        /// Heap flags.
        Flags32 flags;
        /// \brief
        /// Contains the offset of the \see{Registry}.
        AddressType registryOffset;
        // NOTE: If you add new fields, adjust the SIZE and increment
        // the version of your derivative and add if statements to operator
        // << and >> to read and write them. And don't forget to rebuild
        // all dependent allocators as it's likely they will have based
        // their header extensions by deriving from this one.

        /// \brief
        /// The size of the header on disk.
        static const std::size_t SIZE =
            UI32_SIZE +     // magic
            UI32_SIZE +     // flags
            AddressTypeSize;  // regisryOffset

        /// \brief
        /// ctor.
        /// \param[in] flags_ 0 or FLAGS_SECURE.
        Header (ui16 flags_ = 0) :
                flags (flags_),
                registryOffset (0) {
        #if defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
            flags.Set (FLAGS_BLOCK_USES_MAGIC, true);
        #endif // defined (THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)
        }

        /// \brief
        /// Return true if this heap was created with a version of thekogans_util
        /// that was built with THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC.
        /// \return true == FLAGS_BLOCK_USES_MAGIC is set.
        inline bool IsBlockUsesMagic () const {
            return flags.Test (FLAGS_BLOCK_USES_MAGIC);
        }
        /// \brief
        /// Return true if secure.
        /// \return true == secure.
        inline bool IsSecure () const {
            return flags.Test (FLAGS_SECURE);
        }
    } header;

public:
    /// \brief
    /// Allocator size on disk.
    static const std::size_t SIZE = Header::SIZE;

    /// \brief
    /// ctor.
    /// \param[in] secure true == Clear unused space in blocks.
    Allocator (
        TransactedFile::SharedPtr file = nullptr,
        AddressType offset = 0,
        bool secure = false) :
        Object (file, offset),
        header (secure ? Header::FLAGS_SECURE : 0) {}

    /// \brief
    /// Return true if secure.
    /// \return true == secure.
    inline bool IsSecure () const {
        return header.IsSecure ();
    }

    /// \brief
    /// Return the header.registryOffset;
    /// \return header.registryOffset;
    inline AddressType GetRegistryOffset () const {
        return header.registryOffset;
    }
    /// \brief
    /// Set the header.registryOffset.
    /// \param[in] registryOffset New registryOffset to set.
    inline void SetRegistryOffset (AddressType registryOffset) {
        if (header.registryOffset != registryOffset) {
            header.registryOffset = registryOffset;
            SetDirty ();
        }
    }

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

protected:
    // Serializable
    /// \brief
    /// Read the \see{Header}.
    /// \param[in] serializer \see{Serializer} to read the \see{Header} from.
    virtual void Read (Serializer &serializer) override;
    /// \brief
    /// Write the \see{Header}.
    /// \param[in] serializer \see{Serializer} to write the \see{Header} to.
    virtual void Write (Serializer &serializer) override;

    /// \brief
    /// Needs access to private members.
    friend Serializer &operator << (
        Serializer &serializer,
        const Header &header);
    /// \brief
    /// Needs access to private members.
    friend Serializer &operator >> (
        Serializer &serializer,
        Header &header);

    /// \brief
    /// Allocator is neither copy or move constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Allocator)
};
