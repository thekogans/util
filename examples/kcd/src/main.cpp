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

#include <string>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/BTree.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/Options.h"
#include "thekogans/kcd/Version.h"

using namespace thekogans;
using namespace kcd;

namespace {
    std::string GetLevelsList (const std::string &separator) {
        std::string logLevelList;
        {
            std::list<util::ui32> levels;
            util::LoggerMgr::GetLevels (levels);
            if (!levels.empty ()) {
                std::list<util::ui32>::const_iterator it = levels.begin ();
                logLevelList = util::LoggerMgr::levelTostring (*it++);
                for (std::list<util::ui32>::const_iterator end = levels.end (); it != end; ++it) {
                    logLevelList += separator + util::LoggerMgr::levelTostring (*it);
                }
            }
            else {
                logLevelList = "No LoggerMgr levels defined.";
            }
        }
        return logLevelList;
    }

    util::FileAllocator::SharedPtr fileAllocator;

    struct Root : public util::RefCounted {
        THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Root)

        std::string path;
        util::FileAllocator::PtrType pathBTreeOffset;
        util::FileAllocator::PtrType componentBTreeOffset;
        bool active;
        util::BTree::SharedPtr pathBTree;
        util::BTree::SharedPtr componentBTree;

        Root (
            std::string path_ = std::string (),
            util::FileAllocator::PtrType pathBTreeOffset_ = 0,
            util::FileAllocator::PtrType componentBTreeOffset_ = 0,
            bool active_ = false) :
            path (path_),
            pathBTreeOffset (pathBTreeOffset_),
            componentBTreeOffset (componentBTreeOffset_),
            active (active_) {}

        std::size_t Size () const {
            return
                util::Serializer::Size (path) +
                util::FileAllocator::PTR_TYPE_SIZE +
                util::FileAllocator::PTR_TYPE_SIZE +
                util::Serializer::Size (active);
        }

        void Scan () {
            if (!path.empty ()) {
                if (pathBTree == nullptr) {
                    pathBTree.Reset (
                        new util::BTree (
                            fileAllocator,
                            pathBTreeOffset,
                            util::GUIDKey::TYPE,
                            util::StringValue::TYPE));
                }
                if (componentBTree == nullptr) {
                    componentBTree.Reset (
                        new util::BTree (
                            fileAllocator,
                            componentBTreeOffset,
                            util::StringKey::TYPE,
                            util::GUIDArrayValue::TYPE));
                }
                Scan (path);
            }
        }

    private:
        void Scan (std::string path) {
            util::GUIDKey::SharedPtr pathKey (
                new util::GUIDKey (util::GUID::FromBuffer (path.data (), path.size ())));
            util::StringValue::SharedPtr pathValue (new util::StringValue (path));
            util::BTree::Iterator it;
            if (pathBTree->Insert (pathKey, pathValue, it)) {
                std::list<std::string> components;
                util::Path (path).GetComponents (components);
                std::list<std::string>::const_iterator it = components.begin ();
            #if defined (TOOLCHAIN_OS_Windows)
                // If on Windows, skip over the drive leter.
                ++it;
            #endif // defined (TOOLCHAIN_OS_Windows)
                for (; it != components.end (); ++it) {
                    util::StringKey::SharedPtr componentKey (new util::StringKey (*it));
                    util::GUIDArrayValue::SharedPtr componentValue (
                        new util::GUIDArrayValue (std::vector<util::GUID> {pathKey->key}));
                    util::BTree::Iterator jt;
                    if (!componentBTree->Insert (componentKey, componentValue, jt)) {
                        componentValue = jt.GetValue ();
                        componentValue->value.push_back (pathKey->key);
                        jt.SetValue (componentValue);
                    }
                }
            }
            util::Directory directory (path);
            util::Directory::Entry entry;
            for (bool gotEntry = directory.GetFirstEntry (entry);
                 gotEntry; gotEntry = directory.GetNextEntry (entry)) {
                if (!entry.name.empty () &&
                        entry.type == util::Directory::Entry::Folder &&
                        !util::IsDotOrDotDot (entry.name.c_str ())) {
                    Scan (util::MakePath (path, entry.name));
                }
            }
        }
    };

    util::Serializer &operator << (
            util::Serializer &serializer,
            const Root::SharedPtr &root) {
        serializer <<
            root->path <<
            root->pathBTreeOffset <<
            root->componentBTreeOffset <<
            root->active;
        return serializer;
    }

    util::Serializer &operator >> (
            util::Serializer &serializer,
            Root::SharedPtr &root) {
        root.Reset (new Root);
        serializer >>
            root->path >>
            root->pathBTreeOffset >>
            root->componentBTreeOffset >>
            root->active;
        return serializer;
    }

    struct Roots : public util::ArrayValue<Root::SharedPtr> {
        THEKOGANS_UTIL_DECLARE_SERIALIZABLE (Roots)

        Root::SharedPtr Find (const std::string &path) {
            return nullptr;
        }

        void Add (Root::SharedPtr root) {
        }
    };

    THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (Roots, 1, util::BTree::Value::TYPE)
}

template<>
std::size_t util::Serializer::Size<Root::SharedPtr> (
        const Root::SharedPtr &root) {
    return root->Size ();
}

int main (
        int argc,
        const char *argv[]) {
    Options::Instance ()->Parse (argc, argv, "hvlarp");
    THEKOGANS_UTIL_LOG_RESET (
        Options::Instance ()->logLevel,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (new util::ConsoleLogger);
    THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
    if (Options::Instance ()->help) {
        THEKOGANS_UTIL_LOG_INFO (
            "%s [-h] [-v] [-l:'%s'] -a:[scan|show_roots|delete_root|cd] "
            "[-r:rooot] [-p:pattern]\n\n"
            "h - Display this help message.\n"
            "v - Display version information.\n"
            "l - Set logging level (default is Info).\n"
            "a - Action to perform (default is cd).\n"
            "r - Root (can be repeated).\n"
            "p - Pattern.\n",
            argv[0],
            GetLevelsList (" | ").c_str ());
    }
    else if (Options::Instance ()->version) {
        THEKOGANS_UTIL_LOG_INFO (
            "libthekogans_util - %s\n"
            "%s - %s\n",
            util::GetVersion ().ToString ().c_str (),
            argv[0], kcd::GetVersion ().ToString ().c_str ());
    }
    else {
        THEKOGANS_UTIL_TRY {
            THEKOGANS_UTIL_IMPLEMENT_FILE_ALLOCATOR_POOL_FLUSHER;
            fileAllocator = util::FileAllocator::Pool::Instance ()->GetFileAllocator (
                util::MakePath (util::Path::GetHomeDirectory (), "kcd.db"));
            util::FileAllocator::Registry &registry = fileAllocator->GetRegistry ();
            Roots::SharedPtr roots = registry.GetValue ("roots");
            if (roots == nullptr) {
                roots.Reset (new Roots);
            }
            if (Options::Instance ()->action == "scan") {
                if (!Options::Instance ()->roots.empty ()) {
                    for (std::size_t i = 0,
                            count = Options::Instance ()->roots.size (); i < count; ++i) {
                        std::string absolutePath =
                            util::Path (Options::Instance ()->roots[i]).MakeAbsolute ();
                        Root::SharedPtr root = roots->Find (absolutePath);
                        if (root == nullptr) {
                            root.Reset (new Root (absolutePath));
                            roots->Add (root);
                        }
                        std::cout << "Scanning " << absolutePath << "...";
                        root->Scan ();
                        std::cout << "done\n";
                    }
                }
                else {
                }
            }
            else if (Options::Instance ()->action == "show_roots") {
                for (std::size_t i = 0, count = roots->GetLength (); i < count; ++i) {
                }
            }
            else if (Options::Instance ()->action == "delete_root") {
                if (!Options::Instance ()->roots.empty ()) {
                    for (std::size_t i = 0,
                            count = Options::Instance ()->roots.size (); i < count; ++i) {
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to delete.\n");
                }
            }
            else if (Options::Instance ()->action == "cd") {
                if (!Options::Instance ()->pattern.empty ()) {
                }
                else {
                }
            }
            else {
                THEKOGANS_UTIL_LOG_ERROR ("Must specify a valid action.\n");
            }
            registry.SetValue ("roots", roots);
        }
        THEKOGANS_UTIL_CATCH_AND_LOG
    }

#if 0
    std::string path = argc == 2 ?
        util::Path (argv[1]).MakeAbsolute () :
        util::Path::GetCurrDirectory ();
    if (!util::Path (path).Exists ()) {
        THEKOGANS_UTIL_LOG_ERROR ("Path not found: '%s'\n", path.c_str ());
        return 1;
    }
    THEKOGANS_UTIL_TRY {
        THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;

        BTrees btrees;
        util::FileAllocator::PtrType rootOffset = fileAllocator->GetRootOffset ();
        if (rootOffset != 0) {
            util::FileAllocator::BlockBuffer::SharedPtr buffer =
                fileAllocator->CreateBlockBuffer (rootOffset);
            util::ui32 magic;
            *buffer >> magic;
            if (magic == util::MAGIC32) {
                *buffer >> btrees;
            }
        }
        util::BTree pathBTree (
            fileAllocator,
            btrees.path,
            util::GUIDKey::TYPE,
            util::StringValue::TYPE);
        util::BTree componentBTree (
            fileAllocator,
            btrees.component,
            util::StringKey::TYPE,
            util::GUIDArrayValue::TYPE);
        if (rootOffset == 0) {
            rootOffset = fileAllocator->Alloc (BTrees::SIZE);
            fileAllocator->SetRootOffset (rootOffset);
            util::FileAllocator::BlockBuffer::SharedPtr buffer =
                fileAllocator->CreateBlockBuffer (rootOffset, 0, false);
            btrees.path = pathBTree.GetOffset ();
            btrees.component = componentBTree.GetOffset ();
            *buffer << util::MAGIC32 << btrees;
            fileAllocator->WriteBlockBuffer (*buffer);
        }
        util::Console::Instance ()->PrintString (path.c_str ());
        util::Console::Instance ()->PrintString ("\n");
        ScanRoot (path, 1, pathBTree, componentBTree);
        /*
        util::BTree::Iterator it (new util::StringKey ("shape"));
        for (componentBTree.FindFirst (it); !it.IsFinished (); it.Next ()) {
            util::GUIDArrayValue::SharedPtr componentValue = it.GetValue ();
            for (std::size_t i = 0, count = componentValue->value.size (); i < count; ++i) {
                util::BTree::Iterator jt;
                if (pathBTree.Find (util::GUIDKey (componentValue->value[i]), jt)) {
                    std::cout << jt.GetValue ()->ToString () << "\n";
                }
            }
        }
        */
        /*
        util::BTree::Iterator it;
        for (componentBTree.FindFirst (it); !it.Finished (); it.Next ()) {
            std::cout << it.GetKey ()->ToString () << "\n";
        }
        */
    }
    THEKOGANS_UTIL_CATCH_AND_LOG
#endif
    return 0;
}
