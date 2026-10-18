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

/// \brief
/// Forward declaration of \see{Object} needed by \see{ObjectEvents}.
struct Object;

/// \struct TransactedFile::ObjectEvents TransactedFile.h
/// thekogans/util/TransactedFile.h
///
/// \brief
/// Subscribe to ObjectEvents to receive notifications about file
/// store lifetime. This is useful when you have containing objects
/// (objects that contain other objects). They can register for their
/// 'children' events and be notified when containing child offset changed.
struct _LIB_THEKOGANS_UTIL_DECL ObjectEvents {
    /// \brief
    /// dtor.
    virtual ~ObjectEvents () {}

    /// \brief
    /// \see{Object} allocated a block in the file.
    /// \param[in] object \see{Object} whose offset has become valid.
    virtual void OnTransactedFileObjectAllocRange (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} freed its file block.
    /// \param[in] object \see{Object} whose offset has become invalid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will still point to the old offset (which
    /// has been freed).
    virtual void OnTransactedFileObjectFreeRange (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
};

/// \struct TransactedFile::Object TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// A TransactedFile Object is an object that has allocated at least one block
/// from \see{TransactedFile::Allocator} and participates in \see{TransactedFileEvents}.
struct _LIB_THEKOGANS_UTIL_DECL Object :
        public TransactionParticipant,
        public Serializable,
        public Producer<ObjectEvents> {
    /// \brief
    /// Object is a \see{util::DynamicCreatable} abstract base.
    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Object)

#if defined (THEKOGANS_UTIL_TYPE_Static)
    /// \brief
    /// Register all known derivatives. This method is meant to be added
    /// to as new Allocator derivatives are added to the system.
    static void StaticInit ();
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

protected:
    /// \brief
    /// Our address inside the \see{TransactedFile}.
    AddressSpaceType::AddressType offset;

public:
    Object () :
        offset (0) {}
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} where this object resides.
    /// \param[in] offset_ Offset of the \see{TransactedFile::Allocator::Block}.
    Object (
        TransactedFile::SharedPtr file,
        AddressSpaceType::AddressType offset_ = 0) :
        TransactionParticipant (file),
        offset (offset_) {}
    /// \brief
    /// dtor.
    virtual ~Object () {}

    static DynamicCreatable::SharedPtr CreateObject (
        const char *type,
        TransactedFile::SharedPtr file,
        AddressSpaceType::AddressType offset);

    /// \brief
    /// Return the offset.
    /// \return offset.
    inline AddressSpaceType::AddressType GetOffset () const {
        return offset;
    }

    /// \brief
    /// If the object is dirty, flush it's cache to disk.
    /// \return GetOffset ().
    AddressSpaceType::AddressType ForceFlush ();

    /// \brief
    /// If needed allocate space from \see{TransactedFile::Allocator}.
    virtual void AllocRange ();
    /// \brief
    /// Default free implementation for single block objects.
    /// If your objects contain internal pointers to other
    /// blocks you will need to implement this method and
    /// properly free the containing blocks.
    virtual void FreeRange ();
    /// \brief
    /// Flush the internal cache to file.
    virtual void Flush ();
    /// \brief
    /// Reload the internal cache from file.
    virtual void Reload ();

protected:
    // TransactedFileEvents
    /// \brief
    /// Transaction is commiting. Flush the internal cache to file.
    /// \param[in] file \see{TransactedFile} commiting the transaction.
    virtual void OnTransactedFileTransactionCommit (
        TransactedFile::SharedPtr /*file*/,
        int phase) noexcept override;
    /// \brief
    /// Transaction is aborting. Reload the internal cache from file.
    /// \param[in] file \see{TransactedFile} aborting the transaction.
    virtual void OnTransactedFileTransactionAbort (
        TransactedFile::SharedPtr /*file*/) noexcept override;

    /// \brief
    /// Object is neither copy constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Object)
};
