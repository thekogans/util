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

#if !defined (__thekogans_util_NullLogger_h)
#define __thekogans_util_NullLogger_h

#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Logger.h"

namespace thekogans {
    namespace util {

        /// \struct NullLogger NullLogger.h thekogans/util/NullLogger.h
        ///
        /// \brief
        /// NullLogger is a noop \see{Logger}. Use it suppress logging for one or
        /// more subsystems.

        struct _LIB_THEKOGANS_UTIL_DECL NullLogger : public Logger {
            /// \brief
            /// NullLogger participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (NullLogger)

            /// \ brief
            /// ctor.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            NullLogger (ui32 level = MaxLevel) :
                Logger (level) {}

            // Logger
            /// \brief
            /// Dump an entry to Null::StdStream using appropriate color.
            /// \param[in] subsystem Entry subsystem.
            /// \param[in] level Entry log level (used to get appropriate color).
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            virtual void Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string & /*header*/,
                const std::string & /*message*/) throw () override;

            /// \brief
            /// Flush the logger buffers.
            /// \param[in] timeSpec How long to wait for logger to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void Flush (const TimeSpec & /*timeSpec*/ = TimeSpec::Infinite) override;

            /// \brief
            /// NullLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (NullLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_NullLogger_h)
