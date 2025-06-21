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

#if !defined (__thekogans_pathfinder_Root_h)
#define __thekogans_pathfinder_Root_h

#include <string>
#include <vector>
#include <list>
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BTree.h"
#include "thekogans/pathfinder/Database.h"
#include "thekogans/pathfinder/IgnoreList.h"

namespace thekogans {
    namespace pathfinder {

        struct Root;

        struct RootEvents {
            virtual ~RootEvents () {}

            virtual void OnRootScanBegin (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
            virtual void OnRootScanPath (
                util::RefCounted::SharedPtr<Root> /*root*/,
                const std::string & /*path*/) throw () {}
            virtual void OnRootScanEnd (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}

            virtual void OnRootDeleteBegin (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
            virtual void OnRootDeletedPathBTree (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
            virtual void OnRootDeletedComponentBTree (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
            virtual void OnRootDeleteEnd (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}

            virtual void OnRootFindBegin (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
            virtual void OnRootFindPath (
                util::RefCounted::SharedPtr<Root> /*root*/,
                const std::string & /*path*/) throw () {}
            virtual void OnRootFindEnd (
                util::RefCounted::SharedPtr<Root> /*root*/) throw () {}
        };

        struct Root : public util::Producer<RootEvents> {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Root)

        private:
            std::string path;
            util::BTree::SharedPtr pathBTree;
            util::BTree::SharedPtr componentBTree;
            bool active;

        public:
            Root (
                std::string path_ = std::string (),
                util::FileAllocator::PtrType pathBTreeOffset = 0,
                util::FileAllocator::PtrType componentBTreeOffset = 0,
                bool active_ = true) :
                path (path_),
                pathBTree (
                    new util::BTree (
                        Database::Instance ()->GetFileAllocator (),
                        pathBTreeOffset,
                        util::GUIDKey::TYPE,
                        util::StringValue::TYPE)),
                componentBTree (
                    new util::BTree (
                        Database::Instance ()->GetFileAllocator (),
                        componentBTreeOffset,
                        util::StringKey::TYPE,
                        util::GUIDArrayValue::TYPE)),
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

            inline std::size_t Size () const {
                return
                    util::Serializer::Size (path) +
                    util::FileAllocator::PTR_TYPE_SIZE +
                    util::FileAllocator::PTR_TYPE_SIZE +
                    util::Serializer::Size (active);
            }

            void Scan (IgnoreList::SharedPtr ignoreList);
            void Delete ();

            void Find (
                std::list<std::string>::const_iterator patternBegin,
                std::list<std::string>::const_iterator patternEnd,
                bool ignoreCase,
                bool ordered);

        private:
            void Scan (
                const std::string &path,
                IgnoreList::SharedPtr ignoreList);

            friend util::Serializer &operator << (
                util::Serializer &serializer,
                const Root::SharedPtr &root);
            friend util::Serializer &operator >> (
                util::Serializer &serializer,
                Root::SharedPtr &root);
        };

    } // namespace pathfinder
} // namespace thekogans

template<>
inline std::size_t thekogans::util::Serializer::Size<thekogans::pathfinder::Root::SharedPtr> (
        const thekogans::pathfinder::Root::SharedPtr &root) {
    return root->Size ();
}

#endif // !defined (__thekogans_pathfinder_Root_h)
