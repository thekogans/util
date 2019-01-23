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

#if !defined (__thekogans_util_Logger_h)
#define __thekogans_util_Logger_h

#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        /// \struct Logger Logger.h thekogans/util/Logger.h
        ///
        /// \brief
        /// Logger is an abstract base used to provide the api used by
        /// various LoggerMgr pluggins. See ConsoleLogger, FileLogger,
        /// and RemoteLogger for concrete implementations of this
        /// interface.

        struct _LIB_THEKOGANS_UTIL_DECL Logger :
                public virtual ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Logger>.
            typedef ThreadSafeRefCounted::Ptr<Logger> Ptr;

            /// \brief
            /// \see{LoggerMgr::level} this logger will log up to.
            ui32 level;

            /// \brief
            /// ctor.
            /// \param[in] level_ \see{LoggerMgr::level} this logger will log up to.
            Logger (ui32 level_ = UI32_MAX) :
                level (level_) {}
            /// \brief
            /// dtor.
            virtual ~Logger () {}

            /// \brief
            /// Do whatever is appropriate for this logger to
            /// log the entry. All logger derived classes must
            /// implement this pure virtual function.
            /// \param[in] subsystem Entry subsystem. \see{LoggerMgr}
            /// \param[in] level Entry level. \see{LoggerMgr}
            /// \param[in] header Entry header. \see{LoggerMgr}
            /// \param[in] message Entry message. \see{LoggerMgr}
            virtual void Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string & /*header*/,
                const std::string & /*message*/) throw () = 0;

            /// \brief
            /// Flush the logger buffers. After this function returns,
            /// all log entries should be committed to the substrate
            /// represented by this logger.
            /// \param[in] timeSpec How long to wait for logger to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void Flush (const TimeSpec & /*timeSpec*/ = TimeSpec::Infinite) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Logger_h)
