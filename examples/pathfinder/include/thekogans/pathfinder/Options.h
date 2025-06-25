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

#if !defined (__thekogans_pathfinder_Options_h)
#define __thekogans_pathfinder_Options_h

#include <string>
#include <vector>
#include "thekogans/util/Types.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/CommandLineOptions.h"
#include "thekogans/util/LoggerMgr.h"

namespace thekogans {
    namespace pathfinder {

        /// \struct Options Options.h thekogans/pathfinder/Options.h
        ///
        /// \brief
        /// System wide \see{util::Singleton} containing command line options.
        struct Options :
                public util::Singleton<Options>,
                public util::CommandLineOptions {
            /// \brief
            /// -h was specified on the command line.
            bool help;
            /// \brief
            /// -v was specified on the command line.
            bool version;
            /// \brief
            /// -l:'level' was specified on the command line.
            util::ui32 logLevel;
            /// \brief
            /// -d:'database path' was specified on the command line.
            std::string dbPath;
            /// \brief
            /// -a:'action' was specified on the command line.
            std::string action;
            /// \brief
            /// A list of -r:'root' was specified on the command line.
            std::vector<std::string> roots;
            /// \brief
            /// -p:'pattern' was specified on the command line.
            std::string pattern;
            /// \brief
            /// -f:'ignore file path' was specified on the command line.
            std::string ignorePath;
            /// \brief
            /// -i was specified on the command line.
            bool ignoreCase;
            /// \brief
            /// -o was specified on the command line.
            bool ordered;
            /// \brief
            /// -i:'regex pattern' was specified on the command line.
            std::vector<std::string> ignoreList;

            /// \brief
            /// ctor.
            Options () :
                help (false),
                version (false),
                logLevel (util::LoggerMgr::Info),
                dbPath (util::MakePath (util::Path::GetHomeDirectory (), "pathfinder.db")),
                action ("find"),
                ignoreCase (false),
                ordered (false) {}

            virtual void DoOption (
                char option,
                const std::string &value) override;
            virtual void Epilog () override;
        };

    } // namespace pathfinder
} // namespace thekogans

#endif // !defined (__thekogans_pathfinder_Options_h)
