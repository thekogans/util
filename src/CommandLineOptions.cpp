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

#include <cstring>
#include <iostream>
#include "thekogans/util/Console.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/CommandLineOptions.h"

namespace thekogans {
    namespace util {

        void CommandLineOptions::Parse (
                int argc,
                const char *argv[],
                const char *options) {
            Prolog ();
            // Skip module name
            for (int i = 1; i < argc; ++i) {
                if (argv[i][0] == '-') {
                    char option = argv[i][1];
                    std::string value;
                    if (argv[i][2] == ':') {
                        value = &argv[i][3];
                    }
                    if (strchr (options, option)) {
                        DoOption (option, value);
                    }
                    else {
                        DoUnknownOption (option, value);
                    }
                }
                else {
                    // If not an option, assume path.
                    DoPath (argv[i]);
                }
            }
            Epilog ();
        }

        void CommandLineOptions::DoUnknownOption (
                char option,
                const std::string &value) {
            std::string optionAndValue = std::string ("-") + option;
            if (!value.empty ()) {
                optionAndValue += std::string (":") + value;
            }
            Console::Instance ()->PrintString (
                FormatString (
                    "Unknown option: '%s', skipping.\n",
                    optionAndValue.c_str ()),
                Console::StdErr);
        }

    } // namespace util
} // namespace thekogans
