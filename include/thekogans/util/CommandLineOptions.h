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

#if !defined (__thekogans_util_CommandLineOptions_h)
#define __thekogans_util_CommandLineOptions_h

#include <string>
#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Command line options parser.
        ///
        /// This class assumes that options have the following form:
        ///
        /// -'any printable ascii char'[:option value]
        ///
        /// Any other command line parameter will be treated as a path.
        ///
        /// An example should make everything clear. Suppose your app
        /// has the following command line options:
        ///
        /// -a -c -d -l:[Error | Warning | Info | Debug] -v:path
        ///
        /// Your command line options parser would look like this:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct Options : public util::CommandLineOptions {
        ///     bool archive;
        ///     bool core;
        ///     bool daemonize;
        ///     util::ui32 level;
        ///     std::string volumes;
        ///
        ///     Options () :
        ///         archive (false),
        ///         core (false),
        ///         daemonize (false),
        ///         level (
        ///         #if defined (TOOLCHAIN_CONFIG_Debug)
        ///             util::LoggerMgr::Debug
        ///         #else // defined (TOOLCHAIN_CONFIG_Debug)
        ///             util::LoggerMgr::Info
        ///         #endif // defined (TOOLCHAIN_CONFIG_Debug)
        ///         ) {}
        ///
        ///     virtual void DoOption (char option, const std::string &value) {
        ///         switch (option) {
        ///             case 'a':
        ///                 archive = true;
        ///                 break;
        ///             case 'c':
        ///                 core = true;
        ///                 break;
        ///             case 'd':
        ///                 daemonize = true;
        ///                 break;
        ///             case 'l':
        ///                 level = util::LoggerMgr::stringTolevel (value);
        ///                 break;
        ///             case 'v':
        ///                 volumes = value;
        ///                 break;
        ///             default:
        ///                 // This should never happen.
        ///                 assert (0);
        ///                 break;
        ///         }
        ///     }
        ///     virtual void DoPath (const std::string &value) {}
        /// } options;
        /// options.Parse (argc, argv, "acdlv");
        /// \endcode
        ///
        /// IMPORTANT: If an option value contains spaces, enclose the
        /// whole value in "". Also, there should be no spaces between
        /// '-', option leter, the ':' and its value.

        struct _LIB_THEKOGANS_UTIL_DECL CommandLineOptions {
            /// \brief
            /// dtor.
            virtual ~CommandLineOptions () {}

            /// \brief
            /// Parse command line options, calling appropriate Do...
            /// \param[in] argc Count of values in argv.
            /// \param[in] argv Array of command line options.
            /// \param[in] options List of one letter options that we support.
            void Parse (
                int argc,
                const char *argv[],
                const char *options);

            /// \brief
            /// Called right before the options are parsed.
            /// Do one time initialization here.
            virtual void Prolog () {}
            /// \brief
            /// Called when an option not in options is encoutered.
            /// \param[in] option Illegal option.
            /// \param[in] value Illegal option value.
            virtual void DoUnknownOption (
                char option,
                const std::string &value);
            /// \brief
            /// Called when a know option (one in options) is encoutered.
            /// \param[in] option A valid option.
            /// \param[in] value Optional option value.
            virtual void DoOption (
                char option,
                const std::string &value) {}
            /// \brief
            /// Called when a path is encoutered.
            /// \param[in] path A path.
            virtual void DoPath (const std::string &path) {}
            /// \brief
            /// Called right after the options are parsed.
            /// Do one time teardown here.
            virtual void Epilog () {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_CommandLineOptions_h)
