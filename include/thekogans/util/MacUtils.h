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

#if !defined (__thekogans_util_MacUtils_h)
#define __thekogans_util_MacUtils_h

#include <IOKit/IOReturn.h>
#include <string>

namespace thekogans {
    namespace util {

        /// \brief
        /// Return error description from the given OSStatus.
        /// \param[in] errorCode OSStatus to return description for.
        /// \return Error description from the given OSStatus.
        std::string DescriptionFromOSStatus (OSStatus errorCode);

        /// \brief
        /// Return error description from the given CFError.
        /// \param[in] error CFError to return description for.
        /// \return Error description from the given CFError.
        std::string DescriptionFromCFError (CFErrorRef error);

        /// \brief
        /// Return error description from the given IOReturn.
        /// \param[in] errorCode IOReturn to return description for.
        /// \return Error description from the given IOReturn.
        std::string DescriptionFromIOReturn (IOReturn errorCode);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MacUtils_h)
