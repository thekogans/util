// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_stream.
//
// libthekogans_stream is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_stream is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_stream. If not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <fstream>
#include "thekogans/util/StringUtils.h"
#include "thekogans/pathfinder/Options.h"

namespace thekogans {
    namespace pathfinder {

        void Options::DoOption (
                char option,
                const std::string &value) {
            switch (option) {
                case 'h':
                    help = true;
                    break;
                case 'v':
                    version = true;
                    break;
                case 'l':
                    logLevel = util::LoggerMgr::stringTolevel (value.c_str ());
                    break;
                case 'd':
                    dbPath = value;
                    break;
                case 'a':
                    action = value;
                    break;
                case 'r':
                    roots.push_back (value);
                    break;
                case 'p':
                    pattern = value;
                    break;
                case 'f':
                    ignorePath = value;
                    break;
                case 'i':
                    if (value.empty ()) {
                        ignoreCase = true;
                    }
                    else {
                        ignoreList.push_back (value);
                    }
                    break;
                case 'o':
                    ordered = true;
                    break;
            }
        }

        void Options::Epilog () {
            if (!ignorePath.empty ()) {
                std::ifstream ignoreFile (ignorePath.c_str ());
                if (ignoreFile.is_open ()) {
                    while (ignoreFile.good ()) {
                        std::string ignore;
                        std::getline (ignoreFile, ignore);
                        ignore = util::TrimSpaces (ignore.c_str ());
                        if (!ignore.empty ()) {
                            ignoreList.push_back (ignore);
                        }
                    }
                    ignoreFile.close ();
                }
            }
        }

    } // namespace pathfinder
} // namespace thekogans
