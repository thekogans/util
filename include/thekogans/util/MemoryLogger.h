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

#if !defined (__thekogans_util_MemoryLogger_h)
#define __thekogans_util_MemoryLogger_h

#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Logger.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/File.h"

namespace thekogans {
    namespace util {

        /// \struct MemoryLogger MemoryLogger.h thekogans/util/MemoryLogger.h
        ///
        /// \brief
        /// A pluggable Logger instance used to store log entries in memory.

        struct _LIB_THEKOGANS_UTIL_DECL MemoryLogger : public Logger {
            /// \brief
            /// MemoryLogger participates in the \see{DynamicCreatable}
            /// dynamic discovery and creation.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (MemoryLogger)

            /// \brief
            /// Max entries to keep in memory before dropping the oldest.
            std::size_t maxEntries;
            /// \brief
            /// Event handler
            // FIXME: implement.
            /// \brief
            /// Forward declaration of Entry.
            struct Entry;
            /// \brief
            /// Alias for IntrusiveList<Entry>.
            using EntryList = IntrusiveList<Entry>;
            /// \struct LoggerMgr::Entry LoggerMgr.h thekogans/util/LoggerMgr.h
            ///
            /// \brief
            /// Log entry.
            struct Entry : public EntryList::Node {
                /// \brief
                /// Entry has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Alias for std::unique_ptr<Entry>.
                using UniquePtr = std::unique_ptr<Entry>;

                /// \brief
                /// Subsystem that generated this log entry.
                std::string subsystem;
                /// \brief
                /// Entry log level.
                ui32 level;
                /// \brief
                /// Entry header.
                std::string header;
                /// \brief
                /// Entry message.
                std::string message;

                /// \brief
                /// ctor.
                /// \param[in] subsystem_ Subsystem that generated this log entry.
                /// \param[in] level_ Entry log level.
                /// \param[in] header_ Entry header.
                /// \param[in] message_ Entry message.
                Entry (
                    const std::string &subsystem_,
                    ui32 level_,
                    const std::string &header_,
                    const std::string &message_) :
                    subsystem (subsystem_),
                    level (level_),
                    header (header_),
                    message (message_) {}

                /// \brief
                /// Return the serialized size of this log entry.
                /// \return Serialized size of this log entry.
                inline std::size_t Size () const {
                    return
                        Serializer::Size (subsystem) +
                        Serializer::Size (level) +
                        Serializer::Size (header) +
                        Serializer::Size (message);
                }

                /// \brief
                /// Entry is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Entry)
            };
            /// \brief
            /// Log entries.
            EntryList entryList;
            /// \brief
            /// Synchronization spin lock.
            SpinLock spinLock;

            /// \brief
            /// Default maximum number of entries.
            static const std::size_t DEFAULT_MAX_ENTRIES = 1000;

            /// \brief
            /// ctor.
            /// \param[in] maxEntries_ Max entries to keep in memory before dropping the oldest.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            MemoryLogger (
                std::size_t maxEntries_ = DEFAULT_MAX_ENTRIES,
                ui32 level = MaxLevel) :
                Logger (level),
                maxEntries (maxEntries_) {}
            /// \brief
            /// dtor.
            virtual ~MemoryLogger ();

            // Logger
            /// \brief
            /// Save the log entry.
            /// \param[in] subsystem Entry subsystem.
            /// \param[in] level Entry log level (used to get appropriate color).
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            virtual void Log (
                const std::string &subsystem,
                ui32 level,
                const std::string &header,
                const std::string &message) noexcept override;

            /// \brief
            /// Save (and optioally clear) the entries to a file.
            /// \param[in] path File path.
            /// \param[in] flags File open flags.
            /// \param[in] clear true == clear entry list after saving.
            void SaveEntries (
                const std::string &path,
                i32 flags = SimpleFile::ReadWrite | SimpleFile::Create | SimpleFile::Append,
                bool clear = true);

        private:
            /// \brief
            /// Clear entryList.
            void ClearEntries ();

            /// \brief
            /// MemoryLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (MemoryLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MemoryLogger_h)
