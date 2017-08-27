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
#include "thekogans/util/Heap.h"
#include "thekogans/util/Logger.h"
#include "thekogans/util/IntrusiveList.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct MemoryLogger MemoryLogger.h thekogans/util/MemoryLogger.h
        ///
        /// \brief
        /// A pluggable Logger instance used to store log entries in memory.

        struct _LIB_THEKOGANS_UTIL_DECL MemoryLogger : public Logger {
            /// \brief
            /// Max entries to keep in memory before dropping the oldest.
            ui32 maxEntries;
            /// \brief
            /// Event handler
            // FIXME: implement.
            /// \brief
            /// Forward declaration of Entry.
            struct Entry;
            enum {
                /// \brief
                /// EntryList list id.
                ENTRY_LIST_ID
            };
            /// \brief
            /// Convenient typedef for IntrusiveList<Page, ENTRY_LIST_ID>.
            typedef IntrusiveList<Entry, ENTRY_LIST_ID> EntryList;
            /// \struct LoggerMgr::Entry LoggerMgr.h thekogans/util/LoggerMgr.h
            ///
            /// \brief
            /// Log entry.
            struct Entry : public EntryList::Node {
                /// \brief
                /// Entry has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (Entry, SpinLock)

                /// \brief
                /// Convenient typedef for std::unique_ptr<Entry>.
                typedef std::unique_ptr<Entry> UniquePtr;

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
                inline ui32 Size () const {
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

            enum {
                DEFAULT_MAX_ENTRIES = 1000
            };

            /// \brief
            /// ctor.
            /// \param[in] maxEntries_ Max entries to keep in memory before dropping the oldest.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            MemoryLogger (
                ui32 maxEntries_ = DEFAULT_MAX_ENTRIES,
                ui32 level = UI32_MAX) :
                Logger (level),
                maxEntries (maxEntries_) {}

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
                const std::string &message) throw ();

            /// \brief
            /// MemoryLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (MemoryLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MemoryLogger_h)
