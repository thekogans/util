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

#if !defined (__thekogans_kcd_Root_h)
#define __thekogans_kcd_Root_h

#include <string>
#include <vector>
#include <list>
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Subscriber.h"
#include "thekogans/util/Producer.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/kcd/IgnoreList.h"

namespace thekogans {
    namespace kcd {

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
        };

        struct Root : public util::Producer<RootEvents> {
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

            inline std::size_t Size () const {
                return
                    util::Serializer::Size (path) +
                    util::FileAllocator::PTR_TYPE_SIZE +
                    util::FileAllocator::PTR_TYPE_SIZE +
                    util::Serializer::Size (active);
            }

            void Scan (
                util::FileAllocator::SharedPtr fileAllocator,
                util::BufferedFile::Transaction::SharedPtr transaction,
                IgnoreList::SharedPtr ignoreList);
            void Delete (util::FileAllocator::SharedPtr fileAllocator);

            void Find (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &prefix,
                bool ignoreCase,
                std::vector<std::string> &paths);

        private:
            void Scan (
                const std::string &path,
                util::BTree &pathBTree,
                util::BTree &componentBTree,
                IgnoreList::SharedPtr ignoreList);

            friend util::Serializer &operator << (
                util::Serializer &serializer,
                const Root::SharedPtr &root);
            friend util::Serializer &operator >> (
                util::Serializer &serializer,
                Root::SharedPtr &root);
        };

        std::list<std::string>::const_iterator FindPrefix (
            std::list<std::string>::const_iterator pathBegin,
            std::list<std::string>::const_iterator pathEnd,
            const std::string &prefix,
            bool ignoreCase);

    } // namespace kcd
} // namespace thekogans

template<>
inline std::size_t thekogans::util::Serializer::Size<thekogans::kcd::Root::SharedPtr> (
        const thekogans::kcd::Root::SharedPtr &root) {
    return root->Size ();
}

#endif // !defined (__thekogans_kcd_Root_h)
