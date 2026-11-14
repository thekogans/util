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

/// \struct TransactedFile::ObjectEvents TransactedFileObject.h
/// thekogans/util/TransactedFileObject.h
///
/// \brief
/// Subscribe to ObjectEvents to receive notifications about file
/// store lifetime. This is useful when you have containing objects
/// (objects that contain other objects). They can register for their
/// children's events and be notified when containing child offset changed.
struct _LIB_THEKOGANS_UTIL_DECL ObjectEvents {
    /// \brief
    /// dtor.
    virtual ~ObjectEvents () {}

    /// \brief
    /// \see{Object} allocated a block in the file.
    /// \param[in] object \see{Object} whose offset has become valid.
    virtual void OnTransactedFileObjectAllocBlock (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
    /// \brief
    /// \see{Object} freed its file block.
    /// \param[in] object \see{Object} whose offset has become invalid.
    /// VERY IMPORTANT SEMANTICS: When you get this notification,
    /// object->GetOffset () will still point to the old offset (which
    /// has been freed).
    virtual void OnTransactedFileObjectFreeBlock (
        RefCounted::SharedPtr<Object> /*object*/) noexcept {}
};

struct ObjectRange;

/// \struct TransactedFile::Object TransactedFileObject.h thekogans/util/TransactedFileObject.h
///
/// \brief
/// A TransactedFile Object is an object that has allocated at least one block
/// from \see{TransactedFile::Allocator} and participates in \see{TransactedFileTransactionEvents}.
struct _LIB_THEKOGANS_UTIL_DECL Object :
        public virtual Serializable,
        public Subscriber<TransactedFileTransactionEvents>,
        public Subscriber<TransactedFileStructureEvents>,
        public Producer<ObjectEvents> {
    /// \brief
    /// Object is a \see{util::DynamicCreatable} abstract base.
    THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Object)

    static constexpr ui32 MAGIC = 0x4F424A21; // 'OBJ!'

protected:
    /// \brief
    /// \see{TransactedFile} whose \see{TransactedFileTransactionEvents}
    /// we're participants of.
    TransactedFile::SharedPtr file;
    /// \brief
    /// Our address inside the \see{TransactedFile}.
    AddressType offset{0};
    SerializableHeader header;
    bool destroyed{false};

    friend struct ObjectRange;

public:
    // -------+----------------+------------------+
    //  file  |   == nullptr   |   != nullptr     |
    // -------+----------------+------------------+
    // offset | zombie state   | brand new object |
    //  == 0  | ctor only.     | set dirty.       |
    // -------+----------------+------------------+
    //        | error state    | a file backed    |
    //  != 0  |                | object, reload.  |
    // -------+----------------+------------------+
    template<typename T, typename... Args>
    static typename T::SharedPtr Create (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset,
            Args&&... args) {
        typename T::SharedPtr obj (new T (std::forward<Args> (args)...));
        obj->Init (file, offset);
        return obj;
    }

    /// \brief
    /// Type deleter method.
    using DeleterType =
        std::function<void (
            TransactedFile::SharedPtr /*file*/,
            AddressType /*offset*/)>;
    /// \brief
    /// Maps type name to it's deleter method.
    using DeleterMapType = std::unordered_map<
        HashedString, DeleterType, HashedString::Hash, HashedString::EqualTo>;

    static DeleterMapType &GetDeleters () {
        static DeleterMapType deleters;
        return deleters;
    }

    static void DestroyObject (
        TransactedFile::SharedPtr file,
        AddressType offset);
    static void FreeObject (
        TransactedFile::SharedPtr file,
        AddressType offset);

#if defined (THEKOGANS_UTIL_TYPE_Static)
    /// \brief
    static void StaticInit ();
#else // defined (THEKOGANS_UTIL_TYPE_Static)
    /// \struct TransactedFile::Object::DeleterMapInitializer TransactedFileObject.h
    /// thekogans/util/TransactedFileObject.h
    ///
    /// \brief
    struct _LIB_THEKOGANS_UTIL_DECL DeleterMapInitializer {
        /// \brief
        /// ctor.
        /// \param[in] type DynamicCreatable type (it's class name).
        /// \param[in] deleter DynamicCreatable creation factory.
        DeleterMapInitializer (
                const char *type,
                DeleterType deleter) {
            DeleterMapType &deleterMap = GetDeleters ();
            deleterMap[type] = deleter;
        }
    };
#endif // defined (THEKOGANS_UTIL_TYPE_Shared)

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

    inline bool IsDestroyed () const {
        return destroyed;
    }

    inline bool IsDirty () {
        return file->Producer<TransactedFileTransactionEvents>::IsSubscribed (*this);
    }
    /// \brief
    /// Subscribe to \see{TransactedFileTransactionEvents}.
    /// \param[in] dirty == true, Subcribe.
    /// dirty == false, Unsubscribe.
    virtual void SetDirty () override;

    virtual void Init (
        TransactedFile::SharedPtr file_,
        AddressType offset_);

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
    // TransactedFileTransactionEvents
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

    virtual void OnTransactedFileOffsetDestroyed (
            TransactedFile::SharedPtr /*file*/,
            AddressSpaceType::AddressType offset_) noexcept override {
        if (offset == offset_) {
            offset = 0;
            destroyed = true;
        }
    }
};

struct _LIB_THEKOGANS_UTIL_DECL ObjectRange : public BlockRange {
    /// \brief
    /// ctor.
    /// \param[in] file \see{TransactedFile} to buffer.
    /// \param[in] offset File offset.
    /// \param[in] type Expecting type when reading.
    /// \param[in] reading true == Range will be used for reading.
    /// false == Range will be used for writting.
    /// \param[in] allocator \see{util::Allocator} if we need to allocate.
    ObjectRange (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset,
            const SerializableHeader &expecting = SerializableHeader (),
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
            BlockRange (file, offset, true, allocator) {
        ui32 magic;
        *this >> magic;
        if (magic != Object::MAGIC) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                "Corrupt TransactedFile::Object: %s @" THEKOGANS_UTIL_UI64_FORMAT,
                expecting.type, offset);
        }
        *this >> context;
        if (!expecting.NeedType () && !context.IsSameType (expecting.type)) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                "Invalid TransactedFile::Object header type: %s, expecting: %s @" THEKOGANS_UTIL_UI64_FORMAT,
                context.type, expecting.type, offset);
        }
        if (!expecting.NeedVersion () && context.version > expecting.version) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                "Invalid TransactedFile::Object header version " THEKOGANS_UTIL_UI64_FORMAT
                " for type: %s, expecting: " THEKOGANS_UTIL_UI64_FORMAT " @" THEKOGANS_UTIL_UI64_FORMAT,
                context.version, context.type, expecting.version, offset);
        }
    }
    ObjectRange (
            TransactedFile::SharedPtr file,
            TransactedFile::AddressType offset,
            Object &object,
            util::Allocator::SharedPtr allocator = DefaultAllocator::Instance ()) :
            BlockRange (file, offset, false, allocator) {
        *this << Object::MAGIC << object.header;
    }
};
