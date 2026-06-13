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
    /// \see{Object} became dirty.
    /// \param[in] object \see{Object} that became dirty.
    virtual void OnTransactedFileObjectDirty (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}

    /// \brief
    /// \see{Object} allocated a block in the file.
    /// \param[in] object \see{Object} whose offset has become valid.
    virtual void OnTransactedFileObjectAlloc (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} freed its file block.
    /// \param[in] object \see{Object} whose offset has become invalid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will still point to the old offset (which been freed).
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
    /// \param[in] offset_ Offset of the \see{TransactedFile::Allocator::Block}.
    Object (
        TransactedFile::SharedPtr file,
        Allocator::PtrType offset_ = 0) :
        TransactionParticipant (file),
        offset (offset_) {}
    /// \brief
    /// dtor.
    virtual ~Object () {}

    /// \brief
    /// Return the \see{TransactedFile::Allocator}.
    /// \return \see{TransactedFile::Allocator}.
    inline Allocator::SharedPtr GetAllocator () const {
        return file->GetAllocator ();
    }

    /// \brief
    /// Return the offset.
    /// \return offset.
    inline Allocator::PtrType GetOffset () const {
        return offset;
    }

    /// \brief
    /// If the object is dirty, flush it's cache to disk.
    /// \return GetOffset ().
    Allocator::PtrType ForceFlush ();

    // TransactedFile::TransactionParticipant
    /// \brief
    /// Set the dirty flag.
    /// \param[in] dirty true == dirty, false == clean.
    /// \return true == the state has transitioned from clean to dirty.
    virtual bool SetDirty (bool dirty) override;

    /// \brief
    /// If needed allocate space from \see{TransactedFile::Allocator}.
    virtual void Alloc ();
    /// \brief
    /// Default free implementation for single block objects.
    /// If your objects contain internal pointers to other
    /// blocks you will need to implement this method and
    /// properly free the containing blocks.
    virtual void Free ();
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
    /// \param[in] phase \see{TransactedFile} implements two phase commit.
    virtual void OnTransactedFileTransactionCommit (
        TransactedFile::SharedPtr /*file*/,
        int phase) noexcept override;
    /// \brief
    /// Transaction is aborting. Reload the internal cache from file.
    /// \param[in] file \see{TransactedFile} aborting the transaction.
    virtual void OnTransactedFileTransactionAbort (
        TransactedFile::SharedPtr /*file*/) noexcept override;

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
    /// Read the object from the given serializer.
    /// \param[in] serializer Serializer to read the object from.
    virtual void Read (Serializer & /*serializer*/) = 0;
    /// \brief
    /// Write the object to the given serializer.
    /// \param[out] serializer Serializer to write the object to.
    virtual void Write (Serializer & /*serializer*/) = 0;

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
    /// \see{Serializable} creation factory parameters.
    DynamicCreatable::ParametersType parameters;
    /// \brief
    /// The \see{Serializable} object itself.
    Serializable::SharedPtr serializable;

public:
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} where the object resides.
    /// \param[in] offset Object's offset.
    /// \param[in] context_ Determines how the object's \see{SerializableHeader}
    /// is read/writen to/from the file.
    /// \param[in] factory_ \see{Serializable} creation factory.
    /// \param[in] parameters_ \see{Serializable} creation factory parameters.
    /// \param[in] serializable_ The \see{Serializable} object itself.
    SerializableObject (
        TransactedFile::SharedPtr file,
        TransactedFile::Allocator::PtrType offset = 0,
        const SerializableHeader &context_ = SerializableHeader (),
        DynamicCreatable::FactoryType factory_ = nullptr,
        DynamicCreatable::ParametersType parameters_ = nullptr,
        Serializable::SharedPtr serializable_ = nullptr) :
        TransactedFile::Object (file, offset),
        context (context_),
        factory (factory_),
        parameters (parameters_),
        serializable (serializable_) {}

    /// \brief
    /// Return the contained \see{Serializable}. Load it null and offset != 0.
    /// \return serializable.
    Serializable::SharedPtr GetSerializable ();
    /// \brief
    /// Set the contained serializable.
    /// NOTE: Does not call SetDirty (true).
    /// \param[in] serializable_ New contained \see{Serializable}.
    void SetSerializable (Serializable::SharedPtr serializable_);

protected:
    // TransactedFile::Object
    /// Optimization for Alloc. \see{Serializable} declares itself
    /// as fixed size by having ClassSize return > 0.
    /// \return true == object is fixed size.
    virtual bool IsFixedSize () const override {
        return serializable != nullptr && serializable->ClassSize () != 0;
    }
    /// \brief
    /// Return \see{Serializable} binary size (including the header).
    /// \return \see{Serializable} binary size.
    virtual std::size_t Size () const noexcept override {
        return serializable != nullptr ? serializable->GetSize (context) : 0;
    }

    /// \brief
    /// Read the \see{Serializable} from the given serializer.
    /// \param[in] serializer \see{Serializer} to read the \see{Serializable} from.
    virtual void Read (Serializer &serializer) override;
    /// \brief
    /// Write the \see{Serializable} to the given serializer.
    /// \param[out] serializer \see{Serializer} to write the \see{Serializable} to.
    virtual void Write (Serializer &serializer) override;
};
