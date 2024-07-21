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

using namespace thekogans;

namespace {
    void PrintTree (
            const std::string &path,
            util::ui32 level) {
        util::Directory directory (path);
        util::Directory::Entry entry;
        for (bool gotEntry = directory.GetFirstEntry (entry);
                gotEntry; gotEntry = directory.GetNextEntry (entry)) {
            if (!entry.name.empty ()) {
                if (entry.type == util::Directory::Entry::Folder) {
                    if (!util::IsDotOrDotDot (entry.name.c_str ())) {
                        for (util::ui32 i = 0; i < level; ++i) {
                            util::Console::Instance ()->PrintString ("  ");
                        }
                        util::Console::Instance ()->PrintString (entry.name.c_str ());
                        util::Console::Instance ()->PrintString ("\n");
                        PrintTree (util::MakePath (path, entry.name), level + 1);
                    }
                }
            }
        }
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
    std::string path = argc == 2 ? argv[1] : util::Path::GetCurrDirectory ();
    if (!util::Path (path).Exists ()) {
        THEKOGANS_UTIL_LOG_ERROR ("Path not found: '%s'\n", path.c_str ());
        return 1;
    }
    THEKOGANS_UTIL_TRY {
        THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
        util::Console::Instance ()->PrintString (path.c_str ());
        util::Console::Instance ()->PrintString ("\n");
        PrintTree (path, 1);
    }
    THEKOGANS_UTIL_CATCH_AND_LOG
    return 0;
}
