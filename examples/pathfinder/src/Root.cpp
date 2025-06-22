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
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/pathfinder/Root.h"

namespace thekogans {
    namespace pathfinder {

        void Root::Scan (IgnoreList::SharedPtr ignoreList) {
            if (!path.empty ()) {
                Delete ();
                Produce (
                    std::bind (
                        &RootEvents::OnRootScanBegin,
                        std::placeholders::_1,
                        this));
                Scan (path, ignoreList);
                Produce (
                    std::bind (
                        &RootEvents::OnRootScanEnd,
                        std::placeholders::_1,
                        this));
            }
        }

        void Root::Delete () {
            Produce (
                std::bind (
                    &RootEvents::OnRootDeleteBegin,
                    std::placeholders::_1,
                    this));
            pathBTree->Delete ();
            Produce (
                std::bind (
                    &RootEvents::OnRootDeletedPathBTree,
                    std::placeholders::_1,
                    this));
            componentBTree->Delete ();
            Produce (
                std::bind (
                    &RootEvents::OnRootDeletedComponentBTree,
                    std::placeholders::_1,
                    this));
            Produce (
                std::bind (
                    &RootEvents::OnRootDeleteEnd,
                    std::placeholders::_1,
                    this));
        }

        namespace {
            std::list<std::string>::const_iterator FindPrefix (
                    std::list<std::string>::const_iterator pathBegin,
                    std::list<std::string>::const_iterator pathEnd,
                    const std::string &prefix,
                    bool ignoreCase) {
                util::StringKey prefixKey (prefix, ignoreCase);
                while (pathBegin != pathEnd) {
                    if (prefixKey.PrefixCompare (util::StringKey (*pathBegin)) == 0) {
                        break;
                    }
                    ++pathBegin;
                }
                return pathBegin;
            }

            bool ScanPattern (
                    std::list<std::string>::const_iterator pathBegin,
                    std::list<std::string>::const_iterator pathEnd,
                    std::list<std::string>::const_iterator patternBegin,
                    std::list<std::string>::const_iterator patternEnd,
                    bool ignoreCase,
                    bool ordered) {
                while (patternBegin != patternEnd) {
                    std::list<std::string>::const_iterator it =
                        FindPrefix (pathBegin, pathEnd, *patternBegin, ignoreCase);
                    if (it == pathEnd) {
                        return false;
                    }
                    // To honor ordered flag pattern components
                    // must come in order in the resulting paths.
                    if (ordered) {
                        pathBegin = ++it;
                    }
                    ++patternBegin;
                }
                return true;
            }
        }

        void Root::Find (
                std::list<std::string>::const_iterator patternBegin,
                std::list<std::string>::const_iterator patternEnd,
                bool ignoreCase,
                bool ordered) {
            Produce (
                std::bind (
                    &RootEvents::OnRootFindBegin,
                    std::placeholders::_1,
                    this));
            util::StringKey originalPrefix (*patternBegin);
            // Multiple different components with the same prefix
            // (Python/Python38-32) can be found in the same path.
            // Make sure we only count it once.
            std::unordered_set<std::string> duplicates;
            // To allow for the ignoreCase flag the database is
            // maintained without regard to case. All searches
            // must be performed caseless too.
            util::BTree::Iterator it (new util::StringKey (*patternBegin, true));
            for (componentBTree->FindFirst (it); !it.IsFinished (); it.Next ()) {
                // To honor the case request we filter out everything that
                // doesn't match.
                if (ignoreCase || originalPrefix.PrefixCompare (*it.GetKey ()) == 0) {
                    util::GUIDArrayValue::SharedPtr componentValue = it.GetValue ();
                    for (std::size_t i = 0,
                             count = componentValue->value.size (); i < count; ++i) {
                        util::BTree::Iterator jt;
                        if (pathBTree->Find (util::GUIDKey (componentValue->value[i]), jt)) {
                            std::string path = jt.GetValue ()->ToString ();
                            std::list<std::string> pathComponents;
                            util::Path (path).GetComponents (pathComponents);
                            // Components are stored caseless but paths are stored
                            // with case in tact. That means that a component might
                            // be pointing to a path with mismatched case.
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
                                    ordered) &&
                                    duplicates.insert (path).second) {
                                Produce (
                                    std::bind (
                                        &RootEvents::OnRootFindPath,
                                        std::placeholders::_1,
                                        this,
                                        path));
                            }
                        }
                    }
                }
            }
            Produce (
                std::bind (
                    &RootEvents::OnRootFindEnd,
                    std::placeholders::_1,
                    this));
        }

        void Root::Scan (
                const std::string &path,
                IgnoreList::SharedPtr ignoreList) {
            util::GUIDKey::SharedPtr pathKey (
                new util::GUIDKey (util::GUID::FromBuffer (path.data (), path.size ())));
            util::StringValue::SharedPtr pathValue (new util::StringValue (path));
            util::BTree::Iterator it;
            if (pathBTree->Insert (pathKey, pathValue, it)) {
                Produce (
                    std::bind (
                        &RootEvents::OnRootScanPath,
                        std::placeholders::_1,
                        this,
                        path));
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
                        if (ignoreList == nullptr || !ignoreList->ShouldIgnore (entry.name)) {
                            Scan (util::MakePath (path, entry.name), ignoreList);
                        }
                    }
                }
            }
            THEKOGANS_UTIL_CATCH_ANY {
                // Skip over any directories we can't visit. (permissions?)
            }
        }

        util::Serializer &operator << (
                util::Serializer &serializer,
                const Root::SharedPtr &root) {
            serializer <<
                root->path <<
                root->pathBTree->GetOffset () <<
                root->componentBTree->GetOffset () <<
                root->active;
            return serializer;
        }

        util::Serializer &operator >> (
                util::Serializer &serializer,
                Root::SharedPtr &root) {
            std::string path;
            util::FileAllocator::PtrType pathBTreeOffset;
            util::FileAllocator::PtrType componentBTreeOffset;
            bool active;
            serializer >> path >> pathBTreeOffset >> componentBTreeOffset >> active;
            root.Reset (new Root (path, pathBTreeOffset, componentBTreeOffset, active));
            return serializer;
        }

    } // namespace pathfinder
} // namespace thekogans
