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
#include <algorithm>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
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

        void Scan (util::FileAllocator::SharedPtr fileAllocator) {
            if (!path.empty ()) {
                Init (fileAllocator);
                Scan (path);
            }
        }

        void Find (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &prefix,
                std::vector<std::string> &paths) {
            Init (fileAllocator);
            util::BTree::Iterator it (new util::StringKey (prefix));
            for (componentBTree->FindFirst (it); !it.IsFinished (); it.Next ()) {
                util::GUIDArrayValue::SharedPtr componentValue = it.GetValue ();
                for (std::size_t i = 0, count = componentValue->value.size (); i < count; ++i) {
                    util::BTree::Iterator jt;
                    if (pathBTree->Find (util::GUIDKey (componentValue->value[i]), jt)) {
                        paths.push_back (jt.GetValue ()->ToString ());
                    }
                }
            }
        }

    private:
        void Scan (const std::string &path) {
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
            THEKOGANS_UTIL_TRY {
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
            THEKOGANS_UTIL_CATCH_ANY {
            }
        }

        void Init (util::FileAllocator::SharedPtr fileAllocator) {
            if (pathBTree == nullptr) {
                pathBTree.Reset (
                    new util::BTree (
                        fileAllocator,
                        pathBTreeOffset,
                        util::GUIDKey::TYPE,
                        util::StringValue::TYPE));
                if (pathBTreeOffset == 0) {
                    pathBTreeOffset = pathBTree->GetOffset ();
                }
            }
            if (componentBTree == nullptr) {
                componentBTree.Reset (
                    new util::BTree (
                        fileAllocator,
                        componentBTreeOffset,
                        util::StringKey::TYPE,
                        util::GUIDArrayValue::TYPE));
                if (componentBTreeOffset == 0) {
                    componentBTreeOffset = componentBTree->GetOffset ();
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

        void Find (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &prefix,
                std::vector<std::string> &paths) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                value[i]->Find (fileAllocator, prefix, paths);
            }
        }

        Root::SharedPtr Find (const std::string &path) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->path == path) {
                    return value[i];
                }
            }
            return nullptr;
        }

        void Add (Root::SharedPtr root) {
            value.push_back (root);
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
    Options::Instance ()->Parse (argc, argv, "hvlarpo");
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
            "p - Pattern.\n"
            "o - Pattern should appear ordered in the results.\n",
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
            util::FileAllocator::SharedPtr fileAllocator =
                util::FileAllocator::Pool::Instance ()->GetFileAllocator (
                    util::MakePath (util::Path::GetHomeDirectory (), "kcd.db"));
            util::FileAllocator::Registry &registry = fileAllocator->GetRegistry ();
            bool rootsDirty = false;
            Roots::SharedPtr roots = registry.GetValue ("roots");
            if (roots == nullptr) {
                roots.Reset (new Roots);
                rootsDirty = true;
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
                            rootsDirty = true;
                        }
                        std::cout << "Scanning " << absolutePath << "...";
                        root->Scan (fileAllocator);
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
                    std::vector<std::string> result;
                    std::list<std::string> components;
                    util::Path (Options::Instance ()->pattern).GetComponents (components);
                    std::list<std::string>::const_iterator it = components.begin ();
                    if (it != components.end ()) {
                        roots->Find (fileAllocator, *it++, result);
                        std::sort (result.begin (), result.end ());
                        while (it != components.end () && !result.empty ()) {
                            std::vector<std::string> paths;
                            roots->Find (fileAllocator, *it++, paths);
                            std::sort (paths.begin (), paths.end ());
                            std::vector<std::string> temp;
                            for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                                if (std::binary_search (result.begin (), result.end (), paths[i])) {
                                    temp.push_back (paths[i]);
                                }
                            }
                            result.swap (temp);
                            std::sort (result.begin (), result.end ());
                        }
                    }
                    if (!Options::Instance ()->ordered) {
                        for (std::size_t i = 0, count = result.size (); i < count; ++i) {
                            std::cout << result[i] << "\n";
                        }
                    }
                    else {
                        auto FindPrefix = [] (
                                std::list<std::string>::const_iterator begin,
                                std::list<std::string>::const_iterator end,
                                const std::string &prefix) -> std::list<std::string>::const_iterator {
                            while (begin != end) {
                                if (util::StringCompare (
                                        prefix.c_str (), begin->c_str (), prefix.size ()) == 0) {
                                    break;
                                }
                                ++begin;
                            }
                            return begin;
                        };
                        auto ScanComponents = [&] (
                                std::list<std::string>::const_iterator begin,
                                std::list<std::string>::const_iterator end,
                                std::list<std::string>::const_iterator componentsBegin,
                                std::list<std::string>::const_iterator componentsEnd) -> bool {
                            while (componentsBegin != componentsEnd) {
                                begin = FindPrefix (begin, end, *componentsBegin);
                                if (begin++ == end) {
                                    return false;
                                }
                                ++componentsBegin;
                            }
                            return true;
                        };
                        for (std::size_t i = 0, count = result.size (); i < count; ++i) {
                            std::list<std::string> resultComponents;
                            util::Path (result[i]).GetComponents (resultComponents);
                            if (ScanComponents (
                                    resultComponents.begin (),
                                    resultComponents.end (),
                                    components.begin (),
                                    components.end ())) {
                                std::cout << result[i] << "\n";
                            }
                        }
                    }
                }
                else {
                }
            }
            else {
                THEKOGANS_UTIL_LOG_ERROR ("Must specify a valid action.\n");
            }
            if (rootsDirty) {
                registry.SetValue ("roots", roots);
            }
        }
        THEKOGANS_UTIL_CATCH_AND_LOG
    }
    return 0;
}
