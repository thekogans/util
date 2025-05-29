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
#include <regex>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/Database.h"
#include "thekogans/kcd/Root.h"

namespace thekogans {
    namespace kcd {

        void Root::Init () {
            if (pathBTree == nullptr) {
                pathBTree.Reset (
                    new util::BTree (
                        Database::Instance ()->GetFileAllocator (),
                        pathBTreeOffset,
                        util::GUIDKey::TYPE,
                        util::StringValue::TYPE));
                util::Subscriber<util::FileAllocatorObjectEvents>::Subscribe (*pathBTree);
            }
            if (componentBTree == nullptr) {
                componentBTree.Reset (
                    new util::BTree (
                        Database::Instance ()->GetFileAllocator (),
                        componentBTreeOffset,
                        util::StringKey::TYPE,
                        util::GUIDArrayValue::TYPE));
                util::Subscriber<util::FileAllocatorObjectEvents>::Subscribe (*componentBTree);
            }
        }

        void Root::Scan (IgnoreList::SharedPtr ignoreList) {
            if (!path.empty ()) {
                Delete ();
                Init ();
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
            if (pathBTreeOffset != 0) {
                util::BTree::Delete (
                    *Database::Instance ()->GetFileAllocator (), pathBTreeOffset);
                pathBTreeOffset = 0;
                Produce (
                    std::bind (
                        &RootEvents::OnRootDeletedPathBTree,
                        std::placeholders::_1,
                        this));
            }
            pathBTree.Reset ();
            if (componentBTreeOffset != 0) {
                util::BTree::Delete (
                    *Database::Instance ()->GetFileAllocator (), componentBTreeOffset);
                componentBTreeOffset = 0;
                Produce (
                    std::bind (
                        &RootEvents::OnRootDeletedComponentBTree,
                        std::placeholders::_1,
                        this));
            }
            componentBTree.Reset ();
            Produce (
                std::bind (
                    &RootEvents::OnRootDeleteEnd,
                    std::placeholders::_1,
                    this));
        }

        void Root::Find (
                const std::string &prefix,
                bool ignoreCase,
                std::vector<std::string> &paths) {
            Init ();
            util::StringKey originalPrefix (prefix);
            // To allow for the ignoreCase flag the database is
            // maintained without regard to case. All searches
            // must be performed caseless too.
            util::BTree::Iterator it (new util::StringKey (prefix, true));
            for (componentBTree->FindFirst (it); !it.IsFinished (); it.Next ()) {
                // To honor the case request we filter out everything that
                // doesn't match.
                if (ignoreCase || originalPrefix.PrefixCompare (*it.GetKey ()) == 0) {
                    util::GUIDArrayValue::SharedPtr componentValue = it.GetValue ();
                    for (std::size_t i = 0,
                             count = componentValue->value.size (); i < count; ++i) {
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

    } // namespace kcd
} // namespace thekogans
