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
#include "thekogans/util/Path.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/BTreeKeys.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/util/BTree.h"

using namespace thekogans;

namespace {
    void ScanTree (
            const std::string &path,
            util::ui32 level,
            util::BTree &pathBtree,
            util::BTree &componentBtree) {
        util::GUIDKey::SharedPtr pathKey (
            new util::GUIDKey (util::GUID::FromBuffer (path.data (), path.size ())));
        util::StringValue::SharedPtr pathValue (new util::StringValue (path));
        util::BTree::Iterator it;
        if (pathBtree.Insert (pathKey, pathValue, it)) {
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
                    new util::GUIDArrayValue (pathKey->key));
                util::BTree::Iterator jt;
                if (!componentBtree.Insert (componentKey, componentValue, jt)) {
                    componentValue = jt.GetValue ();
                    componentValue->value.push_back (pathKey->key);
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
                for (util::ui32 i = 0; i < level; ++i) {
                    util::Console::Instance ()->PrintString ("  ");
                }
                util::Console::Instance ()->PrintString (entry.name.c_str ());
                util::Console::Instance ()->PrintString ("\n");
                ScanTree (util::MakePath (path, entry.name), level + 1, pathBtree, componentBtree);
            }
        }
    }

    struct BTrees {
        util::FileAllocator::PtrType path;
        util::FileAllocator::PtrType component;

        static const std::size_t SIZE =
            util::FileAllocator::PTR_TYPE_SIZE +
            util::FileAllocator::PTR_TYPE_SIZE;

        BTrees () :
            path (0),
            component (0) {}
    };

    util::Serializer &operator << (
        util::Serializer &serializer,
            const BTrees &btrees) {
        serializer << btrees.path << btrees.component;
        return serializer;
    }

    util::Serializer &operator >> (
            util::Serializer &serializer,
            BTrees &btrees) {
        serializer >> btrees.path >> btrees.component;
        return serializer;
    }
}

int main (
        int argc,
        char *argv[]) {
    THEKOGANS_UTIL_LOG_INIT (
        util::LoggerMgr::Debug,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (
        util::Logger::SharedPtr (new util::ConsoleLogger));
    std::string path = argc == 2 ?
        util::Path (argv[1]).MakeAbsolute () :
        util::Path::GetCurrDirectory ();
    if (!util::Path (path).Exists ()) {
        THEKOGANS_UTIL_LOG_ERROR ("Path not found: '%s'\n", path.c_str ());
        return 1;
    }
    THEKOGANS_UTIL_TRY {
        THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
        util::FileAllocator::SharedPtr fileAllocator =
            util::FileAllocator::Pool::Instance ()->GetFileAllocator (
                util::MakePath (util::Path::GetHomeDirectory (), "kcd.db"));
        util::FileAllocator::Flusher flusher (fileAllocator);
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
        util::BTree pathBtree (
            fileAllocator,
            btrees.path,
            util::GUIDKey::TYPE,
            util::StringValue::TYPE);
        util::BTree componentBtree (
            fileAllocator,
            btrees.component,
            util::StringKey::TYPE,
            util::GUIDArrayValue::TYPE);
        if (rootOffset == 0) {
            rootOffset = fileAllocator->Alloc (BTrees::SIZE);
            fileAllocator->SetRootOffset (rootOffset);
            util::FileAllocator::BlockBuffer::SharedPtr buffer =
                fileAllocator->CreateBlockBuffer (rootOffset, 0, false);
            btrees.path = pathBtree.GetOffset ();
            btrees.component = componentBtree.GetOffset ();
            *buffer << util::MAGIC32 << btrees;
            fileAllocator->WriteBlockBuffer (*buffer);
        }
        util::Console::Instance ()->PrintString (path.c_str ());
        util::Console::Instance ()->PrintString ("\n");
        ScanTree (path, 1, pathBtree, componentBtree);
    }
    THEKOGANS_UTIL_CATCH_AND_LOG
    return 0;
}
