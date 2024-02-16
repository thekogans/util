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

#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/ConsoleLogger.h"

namespace thekogans {
    namespace util {

        Console::ColorType ConsoleLogger::DefaultColorScheme::GetColorForLevel (ui32 level) {
            return
                level == LoggerMgr::Error ? Console::TEXT_COLOR_RED :
                level == LoggerMgr::Warning ? Console::TEXT_COLOR_YELLOW :
                level == LoggerMgr::Info ? Console::TEXT_COLOR_GREEN :
                level == LoggerMgr::Debug ? Console::TEXT_COLOR_MAGENTA : Console::TEXT_COLOR_WHITE;
        }

        void ConsoleLogger::Log (
                const std::string & /*subsystem*/,
                ui32 level,
                const std::string &header,
                const std::string &message) throw () {
            if (level <= this->level && (!header.empty () || !message.empty ())) {
                Console::Instance ().PrintString (
                    FormatString ("%s%s", header.c_str (), message.c_str ()),
                    stream,
                    colorScheme.get () != nullptr ? colorScheme->GetColor (level) : 0);
            }
        }

    } // namespace util
} // namespace thekogans
