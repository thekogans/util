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

#include <iostream>
#include "thekogans/util/CommandLineOptions.h"
#include "thekogans/util/GUID.h"
#include "thekogans/util/StringUtils.h"

using namespace thekogans;

int main (
        int argc,
        const char *argv[]) {
    struct Options : public util::CommandLineOptions {
        util::ui32 count;
        bool windows;
        bool upperCase;
        bool newLine;

        Options () :
            count (1),
            windows (false),
            upperCase (false),
            newLine (false) {}

        virtual void DoOption (
                char option,
                const std::string &value) {
            switch (option) {
                case 'c':
                    count = util::stringToi32 (value.c_str ());
                    break;
                case 'w':
                    windows = true;
                    break;
                case 'u':
                    upperCase = true;
                    break;
                case 'n':
                    newLine = true;
                    break;
            }
        }
    } options;
    options.Parse (argc, argv, "cwun");
    for (util::ui32 i = 0; i < options.count; ++i) {
        util::GUID guid = util::GUID::FromRandom ();
        if (options.windows) {
            std::cout << guid.ToWindowsGUIDString (options.upperCase);
        }
        else {
            std::cout << guid.ToString (options.upperCase);
        }
        if (options.count > 1 || options.newLine) {
            std::cout << std::endl;
        }
    }
    return 0;
}
