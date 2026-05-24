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

/// \struct TransactedFile::TransactionParticipant TransactedFile.h
/// thekogans/util/TransactedFile.h
///
/// \brief
/// TransactionParticipants are objects that listen to
/// \see{TransactedFileEvents} and are able to flush and reload
/// themselves to and from a \see{TransactedFile}.
struct _LIB_THEKOGANS_UTIL_DECL TransactionParticipant :
        public Subscriber<TransactedFileEvents> {
    /// \brief
    /// Declare \see{RefCounted} pointers.
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (TransactionParticipant)

protected:
    /// \brief
    /// \see{TransactedFile} whose \see{TransactedFileEvents}
    /// we're participants of.
    TransactedFile::SharedPtr file;
    /// \brief
    /// Set if the internal cache is dirty.
    static const ui32 FLAGS_DIRTY = 1;
    /// \brief
    /// Set if we've been deleted.
    static const ui32 FLAGS_DELETED = 2;
    /// \brief
    /// Combination of the above flags.
    Flags32 flags;

public:
    /// \brief
    /// ctor.
    /// \param[in] file_ \see{TransactedFile} we're a transaction participant of.
    TransactionParticipant (TransactedFile::SharedPtr file_) :
        file (file_),
        flags (0) {}
    /// \brief
    /// dtor.
    virtual ~TransactionParticipant () {}

    /// \brief
    /// Return the file.
    /// \return file.
    inline TransactedFile::SharedPtr GetFile () const {
        return file;
    }

    /// \brief
    /// Return dirty.
    /// \return dirty.
    inline bool IsDirty () const {
        return flags.Test (FLAGS_DIRTY);
    }
    /// \brief
    /// Set the dirty flag, preserving the state of the deleted flag.
    /// \param[in] dirty true == dirty, false == clean.
    inline void SetDirty (bool dirty) {
        SetFlag (FLAGS_DIRTY, dirty);
    }

    /// \brief
    /// Return deleted.
    /// \return deleted.
    inline bool IsDeleted () const {
        return flags.Test (FLAGS_DELETED);
    }

    /// \brief
    /// Delete the disk image and reset the internal state.
    void Delete ();

protected:
    // NOTE: The following API abstracts out the protocol called for in
    // OnTransactedFileTransactionCommit, OnTransactedFileTransactionAbort
    // and Delete.

    /// \brief
    /// Allocate space from file.
    virtual void Alloc () = 0;
    /// \brief
    /// Free the on disk image.
    virtual void Free () = 0;
    /// \brief
    /// Flush the internal cache to file.
    virtual void Flush () = 0;
    /// \brief
    /// Reload the internal cache from file.
    virtual void Reload () = 0;
    /// \brief
    /// Reset internal state.
    virtual void Reset () = 0;

    // TransactedFileEvents
    /// \brief
    /// Transaction is commiting. Flush the internal cache to file.
    /// \param[in] file \see{TransactedFile} commiting the transaction.
    /// \param[in] phase \see{TransactedFile} implements two phase commit.
    virtual void OnTransactedFileTransactionCommit (
        TransactedFile::SharedPtr /*file*/,
        int phase) noexcept override;
    /// \brief
    /// Transaction is aborting. Reload the internal cache from file.
    /// \param[in] file \see{TransactedFile} aborting the transaction.
    virtual void OnTransactedFileTransactionAbort (
        TransactedFile::SharedPtr /*file*/) noexcept override;

private:
    /// \brief
    /// Set the deleted flag, preserving the state of the dirty flag.
    /// \param[in] deleted true == deleted, false == alive.
    inline void SetDeleted (bool deleted) {
        SetFlag (FLAGS_DELETED, deleted);
    }
    /// \brief
    /// Set the flags.
    /// \param[in] flags_ New flags value.
    void SetFlag (
        ui32 flag,
        bool on);

    /// \brief
    /// TransactionParticipant is neither copy constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (TransactionParticipant)
};

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

    virtual void OnTransactedFileObjectSetDirty (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}

    /// \brief
    /// \see{Object} allocated a block in the file.
    /// \param[in] object \see{Object} whose offset has become valid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will tell you which block has been freed.
    virtual void OnTransactedFileObjectAlloc (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} freed its file block.
    /// \param[in] object \see{Object} whose offset has become invalid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will tell you which block has been allocated.
    virtual void OnTransactedFileObjectFree (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
};

/// \struct TransactedFile::Object TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// A TransactedFile Object is an object that has allocated at least one block
/// from \see{TransactedFile::Allocator} and participates in \see{TransactedFileEvents}.
struct _LIB_THEKOGANS_UTIL_DECL Object :
        public TransactionParticipant,
        public Producer<ObjectEvents> {
    /// \brief
    /// Object is a \see{util::DynamicCreatable} abstract base.
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Object)

protected:
    /// \brief
    /// Our address inside the \see{TransactedFile}.
    Allocator::PtrType offset;

public:
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} where this object resides.
    /// \param[in] offset_ Offset of the \see{Allocator::Block}.
    Object (
        TransactedFile::SharedPtr file,
        Allocator::PtrType offset_ = 0) :
        TransactionParticipant (file),
        offset (offset_) {}
    /// \brief
    /// dtor.
    virtual ~Object () {}

    /// \brief
    /// Return the file.
    /// \return file.
    inline Allocator::SharedPtr GetAllocator () const {
        return file->allocator;
    }

    /// \brief
    /// Return the offset.
    /// \return offset.
    inline Allocator::PtrType GetOffset () const {
        return offset;
    }

    /// \brief
    /// A convenience method used to synchronize the
    /// in memory cache with on disk image.
    /// \return Offset of the on disk image.
    Allocator::PtrType ForceFlush ();

protected:
    /// \brief
    /// Optimization for Alloc below. If an object declares
    /// itself as fixed size, Alloc will not check the object
    /// block size, only offset. And if offset == 0, it will allocate
    /// a block once and that's it.
    /// \return true == object is fixed size.
    virtual bool IsFixedSize () const {
        return false;
    }
    /// \brief
    /// Return the serializable binary size (not including the header).
    /// \return Serializable binary size.
    virtual std::size_t Size () const noexcept = 0;

    /// \brief
    /// Write the serializable from the given serializer.
    /// \param[in] serializer Serializer to read the serializable from.
    virtual void Read (Serializer & /*serializer*/) = 0;
    /// \brief
    /// Write the serializable to the given serializer.
    /// \param[out] serializer Serializer to write the serializable to.
    virtual void Write (Serializer & /*serializer*/) = 0;

    // TransactedFile::TransactionParticipant
    /// \brief
    /// If needed allocate space from \see{File::Allocator}.
    virtual void Alloc () override;
    /// \brief
    /// Default free implementation for single block objects.
    /// If your objects contain internal pointers to other
    /// blocks you will need to implement this method and
    /// properly free the containing blocks.
    virtual void Free () override;
    /// \brief
    /// Flush the internal cache to file.
    virtual void Flush () override;
    /// \brief
    /// Reload the internal cache from file.
    virtual void Reload () override;

    /// \brief
    /// Object is neither copy constructable, nor assignable.
    THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Object)
};

/// \struct TransactedFile::SerializableObject TransactedFile.h thekogans/util/TransactedFile.h
///
/// \brief
/// SerializableObject combines the power of \see{Serializable} (i.e. dynamic
/// discovery and creation) with \see{Object} (i.e. \see{TransactedFile::Allocator}).
struct _LIB_THEKOGANS_UTIL_DECL SerializableObject : public Object {
    /// \brief
    /// Declare \see{RefCounted} pointers.
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (SerializableObject)

    /// \brief
    /// SerializableObject has a private heap.
    THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

protected:
    /// \brief
    /// Determines how the object's \see{SerializableHeader}
    /// is read/writen to/from the file.
    SerializableHeader context;
    /// \brief
    /// \see{Serializable} creation factory.
    DynamicCreatable::FactoryType factory;
    /// \brief
    /// \see{Serializable} creation factory.
    DynamicCreatable::ParametersType parameters;
    /// \brief
    /// The \see{Serializable} object itself.
    Serializable::SharedPtr object;

public:
    /// \brief
    /// ctor.
    /// \param[in] file
    /// \param[in] offset
    /// \param[in] cotext_
    /// \param[in] factory_
    /// \param[in] parameters_
    /// \param[in] object_
    SerializableObject (
        TransactedFile::SharedPtr file,
        TransactedFile::Allocator::PtrType offset = 0,
        const SerializableHeader &context_ = SerializableHeader (),
        DynamicCreatable::FactoryType factory_ = nullptr,
        DynamicCreatable::ParametersType parameters_ = nullptr,
        Serializable::SharedPtr object_ = nullptr) :
        TransactedFile::Object (file, offset),
        context (context_),
        factory (factory_),
        parameters (parameters_),
        object (object_) {}

    Serializable::SharedPtr GetObject ();
    void SetObject (Serializable::SharedPtr object_);

protected:
    // TransactedFile::TransactionParticipant
    /// \brief
    /// Compulsory implementation of \see{TransactedFile::TransactionParticipant::Reset}.
    /// Every leaf class must have one.
    virtual void Reset () override {
        object.Reset ();
    }

    // TransactedFile::Object
    virtual bool IsFixedSize () const override {
        return object->ClassSize () != 0;
    }
    /// \brief
    /// Return object binary size (including the header).
    /// \return Object binary size.
    virtual std::size_t Size () const noexcept override {
        return object->GetSize (context);
    }

    /// \brief
    /// Read the object from the given serializer.
    /// \param[in] serializer Serializer to read the object from.
    virtual void Read (Serializer &serializer) override;
    /// \brief
    /// Write the object to the given serializer.
    /// \param[out] serializer Serializer to write the object to.
    virtual void Write (Serializer &serializer) override;
};
