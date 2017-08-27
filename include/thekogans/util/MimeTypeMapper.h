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

#if !defined (__thekogans_util_MimeTypeMapper_h)
#define __thekogans_util_MimeTypeMapper_h

#include <map>
#include <list>
#include <string>
#include <iostream>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Name of mime types file.
        _LIB_THEKOGANS_UTIL_DECL extern const char * const MIME_TYPES_TXT;

        /// \struct MimeTypeMapper MimeTypeMapper.h thekogans/util/MimeTypeMapper.h
        ///
        /// \brief
        /// MimeTypeMapper implements a forward and reverse mime type
        /// to file extension map. It is a system wide singleton, and
        /// is meant to be initialized with the contents of
        /// mime_types.txt (maintained by the Apache foundation, and a
        /// copy of which is included with util). MimeTypeToExtensions
        /// gives the forward mapping, while ExtensionToMiMimeType
        /// gives the reverse. Once initialized (with a call to
        /// LoadMimeTypes), the class is thread safe.

        struct _LIB_THEKOGANS_UTIL_DECL MimeTypeMapper :
                public Singleton<MimeTypeMapper, SpinLock> {
            /// \brief
            /// Convenient typedef for std::list<std::string>.
            typedef std::list<std::string> ExtensionList;
            /// \brief
            /// Convenient typedef for std::map<std::string, ExtensionList>.
            typedef std::map<std::string, ExtensionList> MimeTypeMap;
            /// \brief
            /// Convenient typedef for std::map<std::string, std::string>.
            typedef std::map<std::string, std::string> ExtensionMap;
            /// \brief
            /// Forward map (mime type -> extensions)
            MimeTypeMap mimeTypeToExtensions;
            /// \brief
            /// Reverse map (extension -> mime type)
            ExtensionMap extensionToMimeType;

            /// \brief
            /// Read the contents of path, and build the forward and reverse maps.
            /// \param[in] path Path to mime_types.txt
            void LoadMimeTypes (const std::string &path);

            /// \brief
            /// Look up a given mime type, and return a list of
            /// extensions associated with it (forward mapping).
            /// \param[in] mimeType Mime type to look up.
            /// \return List of extensions associated with a given mime type.
            const ExtensionList &MimeTypeToExtensions (
                const std::string &mimeType) const;
            /// \brief
            /// Look up a given extension, and return a the
            /// mime type associated with it (erverse mapping).
            /// \param[in] extension Extension to look up.
            /// \return The mime type associated with the given extension.
            std::string ExtensionToMimeType (const std::string &extension) const;

            /// \brief
            /// Dump the maps to stderr.
            void DumpMaps (std::ostream &stream) const;
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MimeTypeMapper_h)
