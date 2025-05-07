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
#include <list>
#include <unordered_set>
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

std::list<std::string>::const_iterator FindPrefix (
        std::list<std::string>::const_iterator pathBegin,
        std::list<std::string>::const_iterator pathEnd,
        const std::string &prefix,
        bool ignoreCase) {
    while (pathBegin != pathEnd) {
        if ((ignoreCase ?
                util::StringCompareIgnoreCase (
                    prefix.c_str (),
                    pathBegin->c_str (),
                    prefix.size ()) :
                util::StringCompare (
                    prefix.c_str (),
                    pathBegin->c_str (),
                    prefix.size ())) == 0) {
            break;
        }
        ++pathBegin;
    }
    return pathBegin;
}

struct Root : public util::RefCounted {
    THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Root)

private:
    std::string path;
    util::FileAllocator::PtrType pathBTreeOffset;
    util::FileAllocator::PtrType componentBTreeOffset;
    bool active;

public:
    Root (
        std::string path_ = std::string (),
        util::FileAllocator::PtrType pathBTreeOffset_ = 0,
        util::FileAllocator::PtrType componentBTreeOffset_ = 0,
        bool active_ = true) :
        path (path_),
        pathBTreeOffset (pathBTreeOffset_),
        componentBTreeOffset (componentBTreeOffset_),
        active (active_) {}

    inline const std::string &GetPath () const {
        return path;
    }

    inline bool IsActive () const {
        return active;
    }
    inline void SetActive (bool active_) {
        active = active_;
    }

    std::size_t Size () const {
        return
            util::Serializer::Size (path) +
            util::FileAllocator::PTR_TYPE_SIZE +
            util::FileAllocator::PTR_TYPE_SIZE +
            util::Serializer::Size (active);
    }

    void Scan (util::FileAllocator::SharedPtr fileAllocator) {
        if (!path.empty ()) {
            Delete (fileAllocator);
            util::BTree::SharedPtr pathBTree (
                new util::BTree (
                    fileAllocator,
                    pathBTreeOffset,
                    util::GUIDKey::TYPE,
                    util::StringValue::TYPE));
            pathBTreeOffset = pathBTree->GetOffset ();
            util::BTree::SharedPtr componentBTree (
                new util::BTree (
                    fileAllocator,
                    componentBTreeOffset,
                    util::StringKey::TYPE,
                    util::GUIDArrayValue::TYPE));
            componentBTreeOffset = componentBTree->GetOffset ();
            Scan (path, pathBTree, componentBTree);
        }
    }

    void Delete (util::FileAllocator::SharedPtr fileAllocator) {
        if (pathBTreeOffset != 0) {
            util::BTree::Delete (*fileAllocator, pathBTreeOffset);
            pathBTreeOffset = 0;
        }
        if (componentBTreeOffset != 0) {
            util::BTree::Delete (*fileAllocator, componentBTreeOffset);
            componentBTreeOffset = 0;
        }
    }

    void Find (
            util::FileAllocator::SharedPtr fileAllocator,
            const std::string &prefix,
            bool ignoreCase,
            std::vector<std::string> &paths) {
        if (pathBTreeOffset != 0 && componentBTreeOffset != 0) {
            util::BTree::SharedPtr pathBTree (
                new util::BTree (
                    fileAllocator,
                    pathBTreeOffset,
                    util::GUIDKey::TYPE,
                    util::StringValue::TYPE));
            util::BTree::SharedPtr componentBTree (
                new util::BTree (
                    fileAllocator,
                    componentBTreeOffset,
                    util::StringKey::TYPE,
                    util::GUIDArrayValue::TYPE));
            util::StringKey originalPrefix (prefix);
            // To allow for the -i flag (ignore case) the database is maintained
            // without regard to case. All searches must be performed caseless too.
            util::BTree::Iterator it (new util::StringKey (prefix, true));
            for (componentBTree->FindFirst (it); !it.IsFinished (); it.Next ()) {
                // To honor the case request we filter out everything that
                // doesn't match.
                if (ignoreCase || originalPrefix.PrefixCompare (*it.GetKey ()) == 0) {
                    util::GUIDArrayValue::SharedPtr componentValue = it.GetValue ();
                    for (std::size_t i = 0, count = componentValue->value.size (); i < count; ++i) {
                        util::BTree::Iterator jt;
                        if (pathBTree->Find (util::GUIDKey (componentValue->value[i]), jt)) {
                            // Components are stored caseless but paths are stored
                            // with case in tact. That means that a component might
                            // be pointing to a path with mismatched case.
                            if (!ignoreCase) {
                                std::list<std::string> pathComponents;
                                util::Path (jt.GetValue ()->ToString ()).GetComponents (
                                    pathComponents);
                                if (FindPrefix (
                                    #if defined (TOOLCHAIN_OS_Windows)
                                        // If on Windows, skip over the drive leter.
                                        ++pathComponents.begin (),
                                    #else // defined (TOOLCHAIN_OS_Windows)
                                        pathComponents.begin (),
                                    #endif // defined (TOOLCHAIN_OS_Windows)
                                        pathComponents.end (),
                                        prefix,
                                        ignoreCase) == pathComponents.end ()) {
                                    continue;
                                }
                            }
                            paths.push_back (jt.GetValue ()->ToString ());
                        }
                    }
                }
            }
        }
    }

private:
    void Scan (
            const std::string &path,
            util::BTree::SharedPtr pathBTree,
            util::BTree::SharedPtr componentBTree) {
        std::cout << path << "\n";
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
                util::StringKey::SharedPtr componentKey (new util::StringKey (*it, true));
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
                    Scan (util::MakePath (path, entry.name), pathBTree, componentBTree);
                }
            }
        }
        THEKOGANS_UTIL_CATCH_ANY {
            std::cout << "Skipping: " << path << "\n";
        }
    }

    friend util::Serializer &operator << (
        util::Serializer &serializer,
        const Root::SharedPtr &root);
    friend util::Serializer &operator >> (
        util::Serializer &serializer,
        Root::SharedPtr &root);
};

// The following three functions are necessary to make
// Roots::value insertion/extraction work.
template<>
std::size_t util::Serializer::Size<Root::SharedPtr> (const Root::SharedPtr &root) {
    return root->Size ();
}

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

    void ScanRoot (
            const std::string &path,
            util::FileAllocator::SharedPtr fileAllocator) {
        Root::SharedPtr root;
        for (std::size_t i = 0, count = value.size (); i < count; ++i) {
            if (value[i]->GetPath () == path) {
                root = value[i];
                break;
            }
        }
        util::FileAllocator::Transaction transaction (fileAllocator);
        bool created = false;
        if (root == nullptr) {
            root.Reset (new Root (path));
            created = true;
        }
        root->Scan (fileAllocator);
        if (created) {
            value.push_back (root);
        }
        fileAllocator->GetRegistry ().SetValue ("roots", this);
        transaction.Commit ();
    }

    void EnableRoot (
            const std::string &path,
            util::FileAllocator::SharedPtr fileAllocator) {
        for (std::size_t i = 0, count = value.size (); i < count; ++i) {
            if (value[i]->GetPath () == path) {
                if (!value[i]->IsActive ()) {
                    util::FileAllocator::Transaction transaction (fileAllocator);
                    value[i]->SetActive (true);
                    fileAllocator->GetRegistry ().SetValue ("roots", this);
                    transaction.Commit ();
                }
                break;
            }
        }
    }

    void DisableRoot (
            const std::string &path,
            util::FileAllocator::SharedPtr fileAllocator) {
        for (std::size_t i = 0, count = value.size (); i < count; ++i) {
            if (value[i]->GetPath () == path) {
                if (value[i]->IsActive ()) {
                    util::FileAllocator::Transaction transaction (fileAllocator);
                    value[i]->SetActive (false);
                    fileAllocator->GetRegistry ().SetValue ("roots", this);
                    transaction.Commit ();
                }
                break;
            }
        }
    }

    void DeleteRoot (
            const std::string &path,
            util::FileAllocator::SharedPtr fileAllocator) {
        for (std::size_t i = 0, count = value.size (); i < count; ++i) {
            if (value[i]->GetPath () == path) {
                util::FileAllocator::Transaction transaction (fileAllocator);
                value[i]->Delete (fileAllocator);
                value.erase (value.begin () + i);
                fileAllocator->GetRegistry ().SetValue ("roots", this);
                transaction.Commit ();
                break;
            }
        }
    }

    void Find (
            util::FileAllocator::SharedPtr fileAllocator,
            const std::string &prefix,
            bool ignoreCase,
            std::vector<std::string> &paths) {
        for (std::size_t i = 0, count = value.size (); i < count; ++i) {
            if (value[i]->IsActive ()) {
                value[i]->Find (fileAllocator, prefix, ignoreCase, paths);
            }
        }
    }
};

THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (Roots, 1, util::BTree::Value::TYPE)

int main (
        int argc,
        const char *argv[]) {
#if defined (THEKOGANS_UTIL_TYPE_Static)
    util::StaticInit ();
    Roots::StaticInit ();
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
    Options::Instance ()->Parse (argc, argv, "hvldarpio");
    THEKOGANS_UTIL_LOG_RESET (
        Options::Instance ()->logLevel,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (new util::ConsoleLogger);
    THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
    if (Options::Instance ()->help) {
        THEKOGANS_UTIL_LOG_INFO (
            "%s [-h] [-v] [-l:'%s'] [-d:'database path'] "
            "-a:[scan_root|enable_root|disable_root|delete_root|show_roots|cd] "
            "[-r:root] [-p:pattern] [-i] [-o]\n\n"
            "h - Display this help message.\n"
            "v - Display version information.\n"
            "l - Set logging level (default is Info).\n"
            "d - Database path (default is $HOME/kcd.db)).\n"
            "a - Action to perform (default is cd).\n"
            "r - Root (can be repeated).\n"
            "p - Pattern (when action is cd).\n"
            "i - Ignore case (when action is cd).\n"
            "o - Pattern should appear ordered in the results (when action is cd).\n",
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
            util::FileAllocator::SharedPtr fileAllocator =
                util::FileAllocator::Pool::Instance ()->GetFileAllocator (
                    Options::Instance ()->dbPath);
            Roots::SharedPtr roots = fileAllocator->GetRegistry ().GetValue ("roots");
            if (roots == nullptr) {
                roots.Reset (new Roots);
            }
            if (Options::Instance ()->action == "scan_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        roots->ScanRoot (
                            util::Path (roots_[i]).MakeAbsolute (), fileAllocator);
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to scan.\n");
                }
            }
            else if (Options::Instance ()->action == "enable_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        roots->EnableRoot (
                            util::Path (roots_[i]).MakeAbsolute (), fileAllocator);
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to enable.\n");
                }
            }
            else if (Options::Instance ()->action == "disable_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        roots->DisableRoot (
                            util::Path (roots_[i]).MakeAbsolute (), fileAllocator);
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to disable.\n");
                }
            }
            else if (Options::Instance ()->action == "delete_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        roots->DeleteRoot (
                            util::Path (roots_[i]).MakeAbsolute (), fileAllocator);
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to delete.\n");
                }
            }
            else if (Options::Instance ()->action == "show_roots") {
                for (std::size_t i = 0, count = roots->GetLength (); i < count; ++i) {
                    std::cout <<
                        (*roots)[i]->GetPath () << " - " <<
                        ((*roots)[i]->IsActive () ? "enabled" : "disabled") << "\n";
                }
            }
            else if (Options::Instance ()->action == "cd") {
                if (!Options::Instance ()->pattern.empty ()) {
                    auto ScanPattern = [] (
                            std::list<std::string>::const_iterator pathBegin,
                            std::list<std::string>::const_iterator pathEnd,
                            std::list<std::string>::const_iterator patternBegin,
                            std::list<std::string>::const_iterator patternEnd,
                            bool ignoreCase,
                            bool ordered) -> bool {
                        while (patternBegin != patternEnd) {
                            std::list<std::string>::const_iterator it =
                                FindPrefix (pathBegin, pathEnd, *patternBegin, ignoreCase);
                            if (it == pathEnd) {
                                return false;
                            }
                            // To honor -o (ordered flag) pattern components
                            // must come in order in the path.
                            if (ordered) {
                                pathBegin = ++it;
                            }
                            ++patternBegin;
                        }
                        return true;
                    };
                    std::list<std::string> patternComponents;
                    util::Path (Options::Instance ()->pattern).GetComponents (patternComponents);
                    std::list<std::string>::const_iterator patternBegin = patternComponents.begin ();
                    std::list<std::string>::const_iterator patternEnd = patternComponents.end ();
                    bool ignoreCase = Options::Instance ()->ignoreCase;
                    std::vector<std::string> paths;
                    roots->Find (fileAllocator, *patternBegin, ignoreCase, paths);
                    bool ordered = Options::Instance ()->ordered;
                    // If we don't care about order, skip over the first
                    // pattern component because Find just found it.
                    if (!ordered) {
                        ++patternBegin;
                    }
                    // Multiple different components with the same prefix (Python/Python38-32)
                    // can be found in the same path. Make sure we only count it once.
                    std::unordered_set<std::string> results;
                    for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                        std::list<std::string> pathComponents;
                        util::Path (paths[i]).GetComponents (pathComponents);
                        if (ScanPattern (
                            #if defined (TOOLCHAIN_OS_Windows)
                                // If on Windows, skip over the drive leter.
                                ++pathComponents.begin (),
                            #else // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.begin (),
                            #endif // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.end (),
                                patternBegin,
                                patternEnd,
                                ignoreCase,
                                ordered)) {
                            if (results.insert (paths[i]).second) {
                                std::cout << paths[i] << "\n";
                            }
                        }
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify a patern to search for.\n");
                }
            }
            else {
                THEKOGANS_UTIL_LOG_ERROR ("Must specify a valid action.\n");
            }
        }
        THEKOGANS_UTIL_CATCH_AND_LOG
    }
    return 0;
}
