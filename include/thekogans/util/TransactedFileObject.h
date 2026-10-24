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
    /// \see{Object} allocated it's master block.
    /// \param[in] object \see{Object} whose offset has become valid.
    virtual void OnTransactedFileObjectAllocBlock (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} freed its master file block.
    /// \param[in] object \see{Object} whose offset has become invalid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will still point to the old offset (which
    /// has been freed).
    virtual void OnTransactedFileObjectFreeBlock (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} is dirty.
    /// \param[in] object \see{Object} that became dirty.
    virtual void OnTransactedFileObjectDirty (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
};

/// \struct TransactedFile::Object TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// A TransactedFile Object is an object that has allocated at least one block
/// from \see{TransactedFile::Allocator} and participates in \see{TransactedFileEvents}.
struct _LIB_THEKOGANS_UTIL_DECL Object :
        public virtual Serializable,
        public Subscriber<TransactedFileEvents>,
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
    /// \see{TransactedFile} whose \see{TransactedFileEvents}
    /// we're participants of.
    TransactedFile::SharedPtr file;
    /// \brief
    /// Our address inside the \see{TransactedFile}.
    AddressType offset;
    struct Tracker {
        WeakPtr object{nullptr};
        AddressType *offset{nullptr};
    };
    std::vector<Tracker> trackers;

public:
    Object (
        TransactedFile::SharedPtr file_ = nullptr,
        AddressType offset_ = 0) :
        file (file_),
        offset (offset_) {}

    static DynamicCreatable::SharedPtr CreateObject (
        const char *type,
        TransactedFile::SharedPtr file,
        AddressType offset);

    /// \brief
    /// Return the file.
    /// \return file.
    inline TransactedFile::SharedPtr GetFile () const {
        return file;
    }

    /// \brief
    /// Return the offset.
    /// \return offset.
    inline AddressType GetOffset () const {
        return offset;
    }

    inline bool IsDirty () {
        return Subscriber<TransactedFileEvents>::IsSubscribed (*file);
    }
    /// \brief
    /// Subscribe to \see{TransactedFileEvents} and notify subscribers that we're dirty.
    void SetDirty ();

    void Track (SharedPtr object, AddressType *offset) {
        trackers.push_back ({object, offset});
    }

    void Untrack (SharedPtr object) {
        for (std::size_t i = 0, count = trackers.size (); i < count; ++i) {
            if (trackers[i].object == object.Get ()) {
                trackers.erase (trackers.begin () + i);
                break;
            }
        }
    }

    virtual void Init (
            TransactedFile::SharedPtr file_,
            AddressType offset_) noexcept {
        file = file_;
        offset = offset_;
    }

    /// \brief
    /// If needed allocate space from \see{TransactedFile::Allocator}.
    virtual void AllocBlock ();
    /// \brief
    /// Default free implementation for single block objects.
    /// If your objects contain internal pointers to other
    /// blocks you will need to implement this method and
    /// properly free the containing blocks.
    virtual void FreeBlock ();
    /// \brief
    /// Flush the internal cache to file.
    virtual void Flush ();
    /// \brief
    /// Reload the internal cache from file.
    virtual void Reload ();

protected:
    // TransactedFileEvents
    /// \brief
    /// File is commiting. Allocate a block ans flush the internal cache.
    /// \param[in] file \see{TransactedFile} commiting the changes.
    virtual void OnTransactedFileCommit (
        TransactedFile::SharedPtr /*file*/,
        int phase) noexcept override;
    /// \brief
    /// File is aborting. Reload the internal cache.
    /// \param[in] file \see{TransactedFile} aborting the changes.
    virtual void OnTransactedFileAbort (
        TransactedFile::SharedPtr /*file*/) noexcept override;

    /// \brief
    /// Object is neither copy constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Object)
};
