#include <thekogans/util/Path.h>
#include <thekogans/util/TransactedFile.h>
#include <thekogans/util/TransactedFileTypeDictionary.h>
#include <thekogans/util/TransactedFileBTreeAllocator.h>
#include <thekogans/util/TransactedFileBTreeRegistry.h>
#include <thekogans/util/TransactedFileObjects.h>

using namespace thekogans::util;

bool ExpectHeapError (
    TransactedFile *file,
    std::size_t expectedError);

struct ReferenceObject :
        public TransactedFile::Object,
        public Subscriber<TransactedFile::ObjectEvents> {
    THEKOGANS_UTIL_DECLARE_TRANSACTED_FILE_OBJECT (ReferenceObject)

    TransactedFile::AddressType reference{0};
    Object::WeakPtr object{nullptr};

    ReferenceObject (Object::SharedPtr object_ = nullptr) {
        if (object_ != nullptr) {
            TrackObject (*object_);
        }
    }

    void TrackObject (Object &object_) {
        Object::SharedPtr obj = object.GetSharedPtr ();
        if (obj != nullptr) {
            obj->Producer<TransactedFile::ObjectEvents>::Unsubscribe (*this);
        }
        object_.Producer<TransactedFile::ObjectEvents>::Subscribe (*this);
        if (reference != object_.GetOffset ()) {
            reference = object_.GetOffset ();
            SetDirty ();
        }
        object.Reset (&object_);
    }

    // Serializable
    virtual void Read (Serializer &serializer) override {
        ui32 magic;
        serializer >> magic;
        if (THEKOGANS_UTIL_UNLIKELY (magic != MAGIC32)) {
            THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                "Corrupt ReferenceObject @" THEKOGANS_UTIL_UI64_FORMAT,
                offset);
        }
        serializer >> reference;
    }
    virtual void Write (Serializer &serializer) override {
        serializer << MAGIC32 << reference;
    }

    // TransactedFile::Object
    virtual void GetBlockMap (
            BlockMap &allocatedBlockMap,
            BlockMap &referencedBlockMap) override {
        Object::GetBlockMap (allocatedBlockMap, referencedBlockMap);
        RecordReference (file, reference, referencedBlockMap);
    }

    // Subscriber<TransactedFileObjectEvents>
    virtual void OnTransactedFileObjectAllocBlock (Object::SharedPtr object) noexcept override {
        if (reference != object->GetOffset ()) {
            reference = object->GetOffset ();
            SetDirty ();
        }
    }

    static constexpr std::size_t SIZE = UI32_SIZE + TransactedFile::AddressTypeSize;
};

THEKOGANS_UTIL_IMPLEMENT_TRANSACTED_FILE_OBJECT (ReferenceObject, 1, ReferenceObject::SIZE)

void RunHeapWalkerReferenceCorruptionTests (const std::string &baseDir) {
    // Helper: create a file with treeA containing ("k1", val1) and ("k2", val2)
    auto makeTreeFile = [&] (
            const std::string &name,
            TransactedFile::AddressType &val1Offset,
            TransactedFile::AddressType &val2Offset) -> std::string {
        const std::string path = MakePath (baseDir, name);
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
        {
            TransactedFile::Transaction tx (file.Get ());
            TransactedFileBTreeRegistry::SharedPtr reg = file->GetRegistry ();
            TransactedFileBTree::SharedPtr treeA = TransactedFile::Object::Create<TransactedFileBTree> (
                file.Get (), 0, StringKey::GetContext (), SerializableHeader (), true);
            reg->InsertValue ("treeA", treeA);

            StringObject::SharedPtr v1 = TransactedFile::Object::Create<StringObject> (file.Get (), 0, "value_one");
            StringObject::SharedPtr v2 = TransactedFile::Object::Create<StringObject> (file.Get (), 0, "value_two");
            treeA->Insert (new StringKey ("k1"), v1);
            treeA->Insert (new StringKey ("k2"), v2);
            tx.Commit ();

            val1Offset = v1->GetOffset ();
            val2Offset = v2->GetOffset ();
        }
        return path;
    };

    // Case 7: ERROR_DANGLING_BLOCK_REFERENCE
    {
        TransactedFile::AddressType val1Offset = 0;
        TransactedFile::AddressType val2Offset = 0;
        const std::string path = makeTreeFile ("corrupt_dangling_ref.db", val1Offset, val2Offset);

        {
            TransactedFile::SharedPtr file = TransactedFile::Open (path);
            // Flip val1's block header to FLAGS_FREE without updating treeA
            TransactedFile::Block blk (file.Get (), val1Offset);
            blk.Read ();
            blk.SetFree (true);
            blk.Write ();
            THEKOGANS_UTIL_ASSERT (
                ExpectHeapError (file.Get (), TransactedFile::HeapWalker::ERROR_DANGLING_BLOCK_REFERENCE),
                "Failed to detect ERROR_DANGLING_BLOCK_REFERENCE!");
        }
    }

    // Case 8: ERROR_DUPLICATE_BLOCK_REFERENCE
    {
        TransactedFile::AddressType val1Offset = 0;
        TransactedFile::AddressType val2Offset = 0;
        const std::string path = makeTreeFile ("corrupt_duplicate_ref.db", val1Offset, val2Offset);

        // Scan raw file for serialized val2Offset and overwrite with val1Offset
        {
            SimpleFile rawFile (HostEndian, path, SimpleFile::ReadWrite);
            std::vector<ui8> buf (rawFile.GetSize ());
            rawFile.Read (buf.data (), buf.size ());
            bool patched = false;
            for (std::size_t i = 0; i + sizeof (TransactedFile::AddressType) <= buf.size (); ++i) {
                TransactedFile::AddressType candidate = 0;
                std::memcpy (&candidate, buf.data () + i, sizeof (candidate));
                if (candidate == val2Offset) {
                    std::memcpy (buf.data () + i, &val1Offset, sizeof (val1Offset));
                    patched = true;
                    break;
                }
            }
            THEKOGANS_UTIL_ASSERT (patched, "Could not locate val2Offset in raw file!");
            rawFile.Seek (0, SEEK_SET);
            rawFile.Write (buf.data (), buf.size ());
        }

        {
            TransactedFile::SharedPtr file = TransactedFile::Open (path);
            THEKOGANS_UTIL_ASSERT (
                ExpectHeapError (file.Get (), TransactedFile::HeapWalker::ERROR_DUPLICATE_BLOCK_REFERENCE),
                "Failed to detect ERROR_DUPLICATE_BLOCK_REFERENCE!");
        }
    }

    // Case 9 & Case 10: ERROR_DANGLING_WEAK_REFERENCE & ERROR_ORPHAN_WEAK_REFERENCE
    // Allocate a WeakRef control block directly via TransactedFile::Object::WeakPtr
    // (or via a test Serializable holding Object::WeakPtr<TransactedFileBTree>)
    {
        const std::string path = MakePath (baseDir, "corrupt_weak_ref.db");
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
        TransactedFileBTreeRegistry::SharedPtr reg = file->GetRegistry ();
        {
            TransactedFile::Transaction tx (file.Get ());
            TransactedFileBTree::SharedPtr treeA = TransactedFile::Object::Create<TransactedFileBTree> (
                file.Get (), 0, StringKey::GetContext (), SerializableHeader (), false);
            reg->InsertValue ("treeA", treeA);
            ReferenceObject::SharedPtr reference = TransactedFile::Object::Create<ReferenceObject> (
                file.Get (), 0, treeA);
            reg->InsertValue ("treeAReference", reference);
            tx.Commit ();
        }
        {
            TransactedFile::Transaction tx (file.Get ());
            reg->RemoveValue ("treeA");
            tx.Commit ();
        }
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (file.Get (), TransactedFile::HeapWalker::ERROR_DANGLING_WEAK_REFERENCE),
            "Failed to detect ERROR_DANGLING_WEAK_REFERENCE!");
    }
    {
        const std::string path = MakePath (baseDir, "corrupt_orphan_weak_ref.db");
        if (Path (path).Exists ()) {
            File::Delete (path);
        }
        TransactedFile::SharedPtr file = TransactedFile::Create (
            HostEndian, path, false,
            new TransactedFileTypeDictionary,
            new TransactedFileBTreeAllocator,
            new TransactedFileBTreeRegistry,
            true);
        TransactedFileBTreeRegistry::SharedPtr reg = file->GetRegistry ();
        {
            TransactedFile::Transaction tx (file.Get ());
            StringObject::SharedPtr string = TransactedFile::Object::Create<StringObject> (
                file.Get (), 0, "foobar");
            ReferenceObject::SharedPtr reference = TransactedFile::Object::Create<ReferenceObject> (
                file.Get (), 0, string);
            reg->InsertValue ("stringReference", reference);
            tx.Commit ();
        }
        THEKOGANS_UTIL_ASSERT (
            ExpectHeapError (file.Get (), TransactedFile::HeapWalker::ERROR_ORPHAN_WEAK_REFERENCE),
            "Failed to detect ERROR_ORPHAN_WEAK_REFERENCE!");
    }
}
