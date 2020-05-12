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

#if !defined (__thekogans_util_FileLogger_h)
#define __thekogans_util_FileLogger_h

#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Logger.h"
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        /// \struct FileLogger FileLogger.h thekogans/util/FileLogger.h
        ///
        /// \brief
        /// A pluggable Logger instance used to dump log entries to a
        /// file. If archive_ = true, the log file is rotated. Two
        /// backups are created (*.1, and *.2). Older archives will be
        /// dropped.

        struct _LIB_THEKOGANS_UTIL_DECL FileLogger : public Logger {
        private:
            /// \brief
            /// Path to a file that will hold the log.
            const std::string path;
            /// \brief
            /// true = rotate the log, false = don't rotate the log.
            bool archive;
            /// \brief
            /// Number of archives before we start dropping.
            std::size_t archiveCount;
            /// \brief
            /// Max log file size before archiving.
            std::size_t maxLogFileSize;
            /// \brief
            /// File to log to.
            SimpleFile file;

        public:
            enum {
                /// \brief
                /// Default number of archives before we start droping.
                DEFAULT_ARCHIVE_COUNT = 2,
                /// \brief
                /// Default max log file size before archiving.
                DEFAULT_MAX_LOG_FILE_SIZE = 2 * 1024 * 1024
            };

            /// \brief
            /// ctor.
            /// \param[in] path_ Path of file to write log entries to.
            /// \param[in] archive_ true = archive the file.
            /// \param[in] archiveCount_ Number of archives before we start droping.
            /// \param[in] maxLogFileSize_ Max log file size before archiving.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            FileLogger (
                const std::string &path_,
                bool archive_ = true,
                std::size_t archiveCount_ = DEFAULT_ARCHIVE_COUNT,
                std::size_t maxLogFileSize_ = DEFAULT_MAX_LOG_FILE_SIZE,
                ui32 level = UI32_MAX) :
                Logger (level),
                path (path_),
                archive (archive_),
                maxLogFileSize (maxLogFileSize_) {}

            // Logger
            /// \brief
            /// Dump an entry to the specified file.
            /// \param[in] subsystem Unused.
            /// \param[in] level Unused.
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            virtual void Log (
                const std::string & /*subsystem*/,
                ui32 /*level*/,
                const std::string &header,
                const std::string &message) throw ();

            /// \brief
            /// Flush the logger buffers.
            /// \param[in] timeSpec How long to wait for logger to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void Flush (const TimeSpec & /*timeSpec*/ = TimeSpec::Infinite) {
                file.Flush ();
            }

        private:
            /// \brief
            /// If archive == true, rotate the log.
            void ArchiveLog ();

            /// \brief
            /// (Re)Open the log file. Create the directory path if it doesn't exist.
            void OpenFile ();

            /// \brief
            /// FileLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (FileLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_FileLogger_h)
