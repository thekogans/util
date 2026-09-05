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

#include <cctype>
#include <iostream>
#include <fstream>
#include "thekogans/util/MimeTypeMapper.h"

namespace thekogans {
    namespace util {

        const char * const MimeTypeMapper::MIME_TYPES_TXT = "mime_types.txt";

        namespace {
            std::size_t SkipSpace (
                    const std::string &line,
                    std::size_t index) {
                for (std::size_t count = line.size (); index < count; ++index) {
                    if (!isspace (line[index])) {
                        break;
                    }
                }
                return index;
            }

            std::string ToLower (const std::string &str) {
                std::string lower;
                lower.resize (str.size ());
                for (std::size_t i = 0, count = str.size (); i < count; ++i) {
                    lower[i] = tolower (str[i]);
                }
                return lower;
            }

            std::string ExrtactWord (
                    const std::string &line,
                    std::size_t &index) {
                std::string word;
                for (std::size_t count = line.size (); index < count; ++index) {
                    if (isspace (line[index])) {
                        break;
                    }
                    word += line[index];
                }
                return ToLower (word);
            }

            void ParseLine (
                    const std::string &line,
                    std::string &mimeType,
                    MimeTypeMapper::ExtensionList &extensions) {
                std::size_t index = SkipSpace (line, 0);
                if (line[index] != '#') {
                    mimeType = ExrtactWord (line, index);
                    if (!mimeType.empty ()) {
                        for (;;) {
                            index = SkipSpace (line, index);
                            if (line[index] == '#') {
                                break;
                            }
                            std::string extension = ExrtactWord (line, index);
                            if (extension.empty ()) {
                                break;
                            }
                            extensions.push_back (extension);
                        }
                    }
                }
            }
        }

        void MimeTypeMapper::LoadMimeTypes (const std::string &path) {
            // Clear out the old maps.
            {
                mimeTypeToExtensions.clear ();
                extensionsToMimeType.clear ();
            }
            // Reload from config file.
            {
                std::ifstream mimeTypesFile (path.c_str ());
                if (mimeTypesFile.is_open ()) {
                    // FIXME: Throw an exception if we cant open the file.
                    while (mimeTypesFile.good ()) {
                        std::string line;
                        std::getline (mimeTypesFile, line);
                        std::string mimeType;
                        ExtensionList extensions;
                        ParseLine (line, mimeType, extensions);
                        if (!mimeType.empty () && !extensions.empty ()) {
                            // FIXME: Should we be checking for collisions here?
                            mimeTypeToExtensions.insert (
                                MimeTypeMap::value_type (mimeType, extensions));
                            for (const auto &extension : extensions) {
                                // FIXME: Should we be checking for collisions here?
                                extensionsToMimeType.insert (
                                    ExtensionMap::value_type (extension, mimeType));
                            }
                        }
                    }
                    mimeTypesFile.close ();
                }
            }
        }

        const MimeTypeMapper::ExtensionList &MimeTypeMapper::MimeTypeToExtensions (
                const std::string &mimeType) const {
            static const ExtensionList *emptyList = new ExtensionList;
            MimeTypeMap::const_iterator it =
                mimeTypeToExtensions.find (ToLower (mimeType));
            return it == mimeTypeToExtensions.end () ? *emptyList : it->second;
        }

        std::string MimeTypeMapper::ExtensionToMimeType (
                const std::string &extension) const {
            ExtensionMap::const_iterator it =
                extensionsToMimeType.find (ToLower (extension));
            return it == extensionsToMimeType.end () ? std::string () : it->second;
        }

        void MimeTypeMapper::DumpMaps (std::ostream &stream) const {
            {
                stream << "*** mimeTypeToExtensions ***" << std::endl;
                for (const auto &mimeTypeToExtension : mimeTypeToExtensions) {
                    stream << mimeTypeToExtension.first << " -> ";
                    {
                        const ExtensionList &extensions = mimeTypeToExtension.second;
                        for (const auto &extension : extensions) {
                            stream << extension << ", ";
                        }
                    }
                    stream << std::endl;
                }
            }
            {
                stream << "*** extensionsToMimeType ***" << std::endl;
                for (const auto &extensionToMimeType : extensionsToMimeType) {
                    stream << extensionToMimeType.first << " -> " << extensionToMimeType.second << std::endl;
                }
            }
            stream.flush ();
        }

    } // namespace util
} // namespace thekogans
