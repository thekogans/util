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
#include <vector>
#include <iostream>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/kcd/Options.h"
#include "thekogans/kcd/Version.h"
#include "thekogans/kcd/Root.h"
#include "thekogans/kcd/Roots.h"
#include "thekogans/kcd/IgnoreList.h"

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

    std::string NormalizePath (const std::string &path) {
        return !path.empty () && path.back () != util::Path::GetNativePathSeparator () ?
            path + util::Path::GetNativePathSeparator () : path;
    }
}

int main (
        int argc,
        const char *argv[]) {
#if defined (THEKOGANS_UTIL_TYPE_Static)
    util::StaticInit ();
    Roots::StaticInit ();
    IgnoreList::StaticInit ();
#endif // defined (THEKOGANS_UTIL_TYPE_Static)
    Options::Instance ()->Parse (argc, argv, "hvldarpfio");
    THEKOGANS_UTIL_LOG_RESET (
        Options::Instance ()->logLevel,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (new util::ConsoleLogger);
    THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
    if (Options::Instance ()->help) {
        THEKOGANS_UTIL_LOG_INFO (
            "%s [-h] [-v] [-l:'%s'] [-d:'database path'] "
            "-a:[scan_root|enable_root|disable_root|delete_root|show_roots|cd|"
            "show_ignore_list|add_ignore|delete_ignore] "
            "[-r:root] [-p:pattern] [-f:'ignore file path'] [-i] [-o]\n\n"
            "h - Display this help message.\n"
            "v - Display version information.\n"
            "l - Set logging level (default is Info).\n"
            "d - Database path (default is $HOME/kcd.db)).\n"
            "a - Action to perform (default is cd).\n"
            "r - Root (can be repeated).\n"
            "p - Pattern (when action is cd).\n"
            "f - Path to ignore file (when action is [add|delete]_ignore).\n"
            "i - Ignore case (when action is cd). Ignore string "
            "(when action is [add|delete]_ignore).\n"
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
            util::FileAllocator::SharedPtr fileAllocator (
                new util::FileAllocator (Options::Instance ()->dbPath));
            Roots::SharedPtr roots = fileAllocator->GetRegistry ().GetValue ("roots");
            if (roots == nullptr) {
                roots.Reset (new Roots);
            }
            IgnoreList::SharedPtr ignoreList =
                fileAllocator->GetRegistry ().GetValue ("ignore_list");
            if (ignoreList == nullptr) {
                ignoreList.Reset (new IgnoreList);
            }
            if (Options::Instance ()->action == "scan_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        roots->ScanRoot (
                            *fileAllocator,
                            guard.transaction,
                            NormalizePath (util::Path (roots_[i]).MakeAbsolute ()),
                            ignoreList);
                        fileAllocator->GetRegistry ().SetValue ("roots", roots);
                        guard.Commit ();
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to scan.\n");
                }
            }
            else if (Options::Instance ()->action == "enable_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    bool commit = false;
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        commit |= roots->EnableRoot (
                            NormalizePath (util::Path (roots_[i]).MakeAbsolute ()));
                    }
                    if (commit) {
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        fileAllocator->GetRegistry ().SetValue ("roots", roots);
                        guard.Commit ();
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one root to enable.\n");
                }
            }
            else if (Options::Instance ()->action == "disable_root") {
                const std::vector<std::string> &roots_ = Options::Instance ()->roots;
                if (!roots_.empty ()) {
                    bool commit = false;
                    for (std::size_t i = 0, count = roots_.size (); i < count; ++i) {
                        commit |= roots->DisableRoot (
                            NormalizePath (util::Path (roots_[i]).MakeAbsolute ()));
                    }
                    if (commit) {
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        fileAllocator->GetRegistry ().SetValue ("roots", roots);
                        guard.Commit ();
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
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        if (roots->DeleteRoot (
                                *fileAllocator,
                                NormalizePath (util::Path (roots_[i]).MakeAbsolute ()))) {
                            fileAllocator->GetRegistry ().SetValue ("roots", roots);
                            guard.Commit ();
                        }
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
                    std::vector<std::string> results;
                    roots->Find (
                        *fileAllocator,
                        Options::Instance ()->pattern,
                        Options::Instance ()->ignoreCase,
                        Options::Instance ()->ordered,
                        results);
                    for (std::size_t i = 0, count = results.size (); i < count; ++i) {
                        std::cout << results[i] << "\n";
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify a patern to search for.\n");
                }
            }
            else if (Options::Instance ()->action == "show_ignore_list") {
                for (std::size_t i = 0, count = ignoreList->GetLength (); i < count; ++i) {
                    std::cout << (*ignoreList)[i] << "\n";
                }
            }
            else if (Options::Instance ()->action == "add_ignore") {
                const std::vector<std::string> &ignoreList_ = Options::Instance ()->ignoreList;
                if (!ignoreList_.empty ()) {
                    bool commit = false;
                    for (std::size_t i = 0, count = ignoreList_.size (); i < count; ++i) {
                        commit |= ignoreList->AddIgnore (ignoreList_[i]);
                    }
                    if (commit) {
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        fileAllocator->GetRegistry ().SetValue ("ignore_list", ignoreList);
                        guard.Commit ();
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one ignore to add.\n");
                }
            }
            else if (Options::Instance ()->action == "delete_ignore") {
                const std::vector<std::string> &ignoreList_ = Options::Instance ()->ignoreList;
                if (!ignoreList_.empty ()) {
                    bool commit = false;
                    for (std::size_t i = 0, count = ignoreList_.size (); i < count; ++i) {
                        commit |= ignoreList->DeleteIgnore (ignoreList_[i]);
                    }
                    if (commit) {
                        util::BufferedFile::Transaction::Guard guard (
                            fileAllocator->CreateTransaction ());
                        fileAllocator->GetRegistry ().SetValue ("ignore_list", ignoreList);
                        guard.Commit ();
                    }
                }
                else {
                    THEKOGANS_UTIL_LOG_ERROR ("Must specify at least one ignore to add.\n");
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
