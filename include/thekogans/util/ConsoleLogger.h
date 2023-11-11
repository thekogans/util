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

#if !defined (__thekogans_util_ConsoleLogger_h)
#define __thekogans_util_ConsoleLogger_h

#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Console.h"
#include "thekogans/util/Logger.h"

namespace thekogans {
    namespace util {

        /// \struct ConsoleLogger ConsoleLogger.h thekogans/util/ConsoleLogger.h
        ///
        /// \brief
        /// A pluggable Logger instance used to dump log entries to \see{Console::StdStream}.
        /// ConsoleLogger uses a color scheme to color code the entries based on their
        /// log level.
        /// VERY IMPORTANT: By instantiating a ConsoleLogger, I assume that the application
        /// is \see{Console} based. Because \see{Console} performs important initialization
        /// in it's ctor required by \see{MainRunLoop} and \see{ChildProcess}. ConsoleLogger
        /// ctor explicitly creates the \see{Console} \see{Singleton}. If you need to supply
        /// custom \see{Console} ctor parameters you need to call \see{Console::CreateInstance}
        /// before instantiating a \see{Console::Instance}.

        struct _LIB_THEKOGANS_UTIL_DECL ConsoleLogger : public Logger {
            /// \struct ConsoleLogger::ColorScheme ConsoleLogger.h thekogans/util/ConsoleLogger.h
            ///
            /// \brief
            /// Color scheme base. Provides colors based on log level.
            struct _LIB_THEKOGANS_UTIL_DECL ColorScheme {
                /// \brief
                /// Convenient typedef for std::unique_ptr<ColorScheme>.
                typedef std::unique_ptr<ColorScheme> UniquePtr;

                /// \brief
                /// dtor.
                virtual ~ColorScheme () {}

                /// \brief
                /// Given a log level, return appropriate color.
                /// \param[in] level Log level.
                /// \return Color for the provided log level.
                virtual Console::ColorType GetColor (ui32 /*level*/) = 0;
            };
            /// \struct ConsoleLogger::DefaultColorScheme ConsoleLogger.h thekogans/util/ConsoleLogger.h
            ///
            /// \brief
            /// Default color scheme base. Provides the
            /// following colors based on log level.
            /// Error - red
            /// Warning - yellow
            /// Info - green
            /// Debug - magenta
            /// Development - white
            struct _LIB_THEKOGANS_UTIL_DECL DefaultColorScheme : public ColorScheme {
                /// \brief
                /// Given a log level, return appropriate color.
                /// \param[in] level Log level.
                /// \return Color for the provided log level.
                static Console::ColorType GetColorForLevel (ui32 level);

                /// \brief
                /// Given a log level, return appropriate color.
                /// \param[in] level Log level.
                /// \return Color for the provided log level.
                virtual Console::ColorType GetColor (ui32 level) override {
                    return GetColorForLevel (level);
                }
            };

        private:
            /// \brief
            /// Where to print the log entry.
            Console::StdStream stream;
            /// \brief
            /// Current color scheme.
            ColorScheme::UniquePtr colorScheme;

        public:
            /// \ brief
            /// ctor.
            /// \param[in] stream_ Where to print the log entry.
            /// \param[in] colorScheme_ Color scheme to use to color the log entries.
            /// \param[in] level \see{LoggerMgr::level} this logger will log up to.
            ConsoleLogger (
                    Console::StdStream stream_ = Console::StdErr,
                    ColorScheme::UniquePtr colorScheme_ =
                        ColorScheme::UniquePtr (new DefaultColorScheme),
                    ui32 level = MaxLevel) :
                    Logger (level),
                    stream (stream_),
                    colorScheme (std::move (colorScheme_)) {
                // This will create the \see{Console} singleton.
                Flush ();
            }

            // Logger
            /// \brief
            /// Dump an entry to Console::StdStream using appropriate color.
            /// \param[in] subsystem Entry subsystem.
            /// \param[in] level Entry log level (used to get appropriate color).
            /// \param[in] header Entry header.
            /// \param[in] message Entry message.
            virtual void Log (
                const std::string & /*subsystem*/,
                ui32 level,
                const std::string &header,
                const std::string &message) throw () override;

            /// \brief
            /// Flush the logger buffers.
            /// \param[in] timeSpec How long to wait for logger to complete.
            /// IMPORTANT: timeSpec is a relative value.
            virtual void Flush (const TimeSpec & /*timeSpec*/ = TimeSpec::Infinite) override {
                Console::Instance ().FlushPrintQueue ();
            }

            /// \brief
            /// ConsoleLogger is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (ConsoleLogger)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_ConsoleLogger_h)
