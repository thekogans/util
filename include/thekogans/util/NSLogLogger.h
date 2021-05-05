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

#if !defined (__thekogans_util_NSLogLogger_h)
#define __thekogans_util_NSLogLogger_h

#if defined (TOOLCHAIN_OS_OSX)

#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Logger.h"

namespace thekogans {
    namespace util {

        /// \struct NSLogLogger NSLogLogger.h thekogans/util/NSLogLogger.h
        ///
        /// \brief
        /// A pluggable Logger instance used to dump log entries to the XCode console.

        struct _LIB_THEKOGANS_UTIL_DECL NSLogLogger : public Logger {
            /// \brief
            /// ctor.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            NSLogLogger (ui32 level = MaxLevel) :
                Logger (level) {}

            // Logger
            /// \brief
            /// Dump an entry to XCode console.
            /// \param[in] subsystem Entry subsystem.
            /// \param[in] level Entry log level.
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            virtual void Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string &header,
                const std::string &message) throw ();

            /// \brief
            /// NSLogLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (NSLogLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_OSX)

#endif // !defined (__thekogans_util_NSLogLogger_h)
