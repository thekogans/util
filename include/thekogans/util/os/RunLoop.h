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

#if !defined (__thekogans_util_os_RunLoop_h)
#define __thekogans_util_os_RunLoop_h

#include "thekogans/util/Config.h"

namespace thekogans {
    namespace util {
        namespace os {

            /// \struct RunLoop OS.h thekogans/util/OS.h
            ///
            /// \brief
            /// Base class for OS based run loop. This interface class is meant to be a mix in class
            /// alowing OS based implementations to define Begin/End/ScheduleJob using whatever
            /// facilities are available on that platform. ExecuteJob is left as an implementation
            /// detail for RunLoop derivatives (see \see{SystemRunLoop} for an example.)

            struct _LIB_THEKOGANS_UTIL_DECL RunLoop {
                /// \brief
                /// dtor.
                virtual ~RunLoop () {}

                // This is system specific. Each RunLoop implementation will use whatever
                // system facilities it needs to start/stop and schedule jobs.

                /// \brief
                /// Start the run loop.
                virtual void Begin () = 0;
                /// \brief
                /// Stop the run loop.
                virtual void End () = 0;
                /// \brief
                /// Schedule a job to be performed by run loop.
                virtual void ScheduleJob () = 0;

                /// \brief
                /// This callback is left as an implementation detail (see \see{SystemRunLoop}).
                /// It is what allows os::Runloop to be mixed in with \see{util::RunLoop}.
                virtual void ExecuteJob () = 0;
            };

        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_os_RunLoop_h)
