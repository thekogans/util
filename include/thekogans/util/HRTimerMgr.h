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

#if !defined (__thekogans_util_HRTimerMgr_h)
#define __thekogans_util_HRTimerMgr_h

#include <string>
#include <list>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/AbstractOwnerList.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/HRTimer.h"
#include "thekogans/util/XMLUtils.h"

namespace thekogans {
    namespace util {

        /// \struct HRTimerMgr HRTimerMgr.h thekogans/util/HRTimerMgr.h
        ///
        /// \brief
        /// HRTimerMgr is a profiling harness. It allows you to setup
        /// very detailed timing measurements. The measurements are
        /// aggregated hierarchically, and results are merged from
        /// callee to caller. This provides very detailed runtime
        /// statistics that are a hell of a lot more accurate than
        /// traditional interval samplers. Instrumenting your code with
        /// HRTimerMgr is very straightforward, and if you use the
        /// macros provided below, the profiling code can be turned
        /// on/off with a switch. This allows you to build diagnostics
        /// in to your code, have your cake (be able to profile and
        /// tune your code), and eat it too (not pay a runtime penalty
        /// in production code). The profiler code is turned on by
        /// defining THEKOGANS_UTIL_USE_HRTIMER_MGR before including
        /// thekogans/util/HRTimerMgr.h.
        ///
        /// Ex: In any *.cpp:
        ///
        /// \code{.cpp}
        /// // Comment out the following define to disable the profiler.
        /// // Or, alternatively, you can define
        /// // THEKOGANS_UTIL_USE_HRTIMER_MGR globally to
        /// // enable/disable profiling for the entire project.
        /// #define THEKOGANS_UTIL_USE_HRTIMER_MGR
        /// #include "thekogans/util/HRTimerMgr.h"
        /// \endcode
        ///
        /// The following example is taken from a piece of production
        /// code used to extract metadata from media files:
        ///
        /// \code{.cpp}
        /// struct ExtractMetadataJob : public thekogans::util::RunLoop::Job {
        ///     // Nonessential members, and methods omitted for clarity.
        ///
        ///     virtual void Execute (volatile const bool &done) throw () {
        ///         THEKOGANS_UTIL_HRTIMER_MGR ("ExtractMetadataJob::Execute");
        ///         {
        ///             THEKOGANS_UTIL_HRTIMER_MGR_SCOPE ("categories");
        ///             VolumeDB volumeDB (requestQueue, false);
        ///             for (thekogans::util::i32 category = db::FolderItem::CATEGORY_FIRST;
        ///                     !done && category < db::FolderItem::CATEGORY_LAST;
        ///                     ++category) {
        ///                 ExtractCategory (THEKOGANS_UTIL_HRTIMER_MGR_GET_COMMA
        ///                     volumeDB, category, done);
        ///             }
        ///         }
        ///     }
        ///
        /// private:
        ///     void ExtractCategory (
        ///             THEKOGANS_UTIL_HRTIMER_MGR_PROTO_COMMA
        ///             const db::DB &db,
        ///             thekogans::util::i32 category,
        ///             volatile const bool &done) {
        ///         assert (category < db::FolderItem::CATEGORY_LAST);
        ///         for (thekogans::util::i32 type = db::FolderItem::TYPE_FIRST;
        ///                 !done && type < db::FolderItem::TYPE_LAST; ++type) {
        ///             for (thekogans::util::OwnerList<db::FolderItem> folderItems; !done &&
        ///                     GetFolderItems (THEKOGANS_UTIL_HRTIMER_MGR_GET_COMMA
        ///                         db, type, category, db::FolderItem::STATUS_INDEXED,
        ///                         BATCH_SIZE, folderItems, done) > 0;
        ///                     folderItems.deleteAndClear ()) {
        ///                 for (std::list<db::FolderItem *>::const_iterator
        ///                         it = folderItems.begin (), end = folderItems.end ();
        ///                         !done && it != end; ++it) {
        ///                     db::Metadata::UniquePtr metadata;
        ///                     {
        ///                         THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER (
        ///                             "db::Metadata::Get (db, db::FolderItem::%s, db::FolderItem::%s, %s);",
        ///                             db::FolderItem::TypeTostring ((*it)->Type ()).c_str (),
        ///                             db::FolderItem::CategoryTostring ((*it)->category).c_str (),
        ///                             (*it)->id.c_str ());
        ///                         metadata = db::Metadata::Get (db,
        ///                             (*it)->Type (), (*it)->category, (*it)->id);
        ///                     }
        ///                     if (metadata.get () != 0) {
        ///                         {
        ///                             THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER (
        ///                                 "metadata->Extract (db, **it);");
        ///                             metadata->Extract (db, **it, done);
        ///                         }
        ///                         {
        ///                             THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_SCOPE (
        ///                                 "DB_TRANSACTION (db);");
        ///                             DB_TRANSACTION (db);
        ///                             {
        ///                                 THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER (
        ///                                     "metadata->Update (db);");
        ///                                 metadata->Update (db);
        ///                             }
        ///                             {
        ///                                 THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER (
        ///                                     "(*it)->UpdateStatus (db, "
        ///                                     "db::FolderItem::STATUS_EXTRACTED);");
        ///                                 (*it)->UpdateStatus (db,
        ///                                     db::FolderItem::STATUS_EXTRACTED);
        ///                             }
        ///                             DB_TRANSACTION_COMMIT;
        ///                         }
        ///                     }
        ///                     else {
        ///                         THEKOGANS_UTIL_LOG_ERROR (
        ///                             "Unable to get metadata for: '%s'.\n",
        ///                             (*it)->path.c_str ());
        ///                         (*it)->UpdateStatus (db,
        ///                             db::FolderItem::STATUS_EXTRACTED);
        ///                     }
        ///                 }
        ///             }
        ///         }
        ///     }
        /// };
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL HRTimerMgr {
        private:
            /// \struct HRTimerMgr::TimerInfoBase HRTimerMgr.h thekogans/util/HRTimerMgr.h
            ///
            /// \brief
            /// TimerInfoBase is the base class for \see{TimerInfo} and
            /// \see{ScopeInfo}. It exposes the api by which the HRTimerMgr
            /// collects the statistics from various scopes.
            struct _LIB_THEKOGANS_UTIL_DECL TimerInfoBase {
                /// \brief
                /// Convenient typedef for std::unique_ptr<TimerInfoBase>.
                typedef std::unique_ptr<TimerInfoBase> UniquePtr;

                /// \btief
                /// Label by which this TimerInfoBase is identified.
                std::string name;
                /// \brief
                /// A list of user defined attributes attached
                /// to this TimerInfoBase.
                Attributes attributes;

                /// \brief
                /// ctor.
                /// \param[in] name_ TimerInfoBase name.
                explicit TimerInfoBase (const std::string &name_) :
                    name (name_) {}
                /// \brief
                /// dtor.
                virtual ~TimerInfoBase () {}

                /// \brief
                /// Return stats accumulated by this TimerInfoBase.
                /// \param[out] count Number of times this block was hit.
                /// \param[out] min Minimum time spent in the block.
                /// \param[out] max Maximum time spent in the block.
                /// \param[out] average Average time spent in the block.
                /// \param[out] total Total time spent in the block.
                virtual void GetStats (
                    ui32 & /*count*/,
                    ui64 & /*min*/,
                    ui64 & /*max*/,
                    ui64 & /*average*/,
                    ui64 & /*total*/) const = 0;
                /// \brief
                /// Format an XML string from TimerInfoBase parameters
                /// and attributes.
                /// \param[in] indentationLevel 4 * indentationLevel
                /// spaces to add to the beginning of the string.
                /// \return An XML string from TimerInfoBase parameters
                /// and attributes.
                virtual std::string ToString (ui32 /*indentationLevel*/) const = 0;

                /// \brief
                /// Add a used defined key/value pair to be included
                /// in the XML string. This feature give you the ability
                /// to add app specific attributes to your timer data.
                /// \param[in] attribute Attribute to add.
                inline void AddAttribute (const Attribute &attribute) {
                    attributes.push_back (attribute);
                }
            };
            /// \struct HRTimerMgr::TimerInfo HRTimerMgr.h thekogans/util/HRTimerMgr.h
            ///
            /// \brief
            /// Simple non hierarchical timer. Used to time one section
            /// of code.
            struct _LIB_THEKOGANS_UTIL_DECL TimerInfo : public TimerInfoBase {
                /// \brief
                /// TimerInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (TimerInfo, SpinLock)

                /// \brief
                /// "Timer"
                static const char * const TAG_TIMER;
                /// \brief
                /// "Name"
                static const char * const ATTR_NAME;
                /// \brief
                /// "Start"
                static const char * const ATTR_START;
                /// \brief
                /// "Stop"
                static const char * const ATTR_STOP;
                /// \brief
                /// "Ellapsed"
                static const char * const ATTR_ELLAPSED;

                /// \brief
                /// Start time.
                ui64 start;
                /// \brief
                /// End time.
                ui64 stop;

                /// \brief
                /// ctor.
                /// \param[in] name Name of this TimerInfo.
                explicit TimerInfo (const std::string &name) :
                    TimerInfoBase (name),
                    start (0),
                    stop (0) {}

                /// \brief
                /// Return stats accumulated by this TimerInfo.
                /// \param[out] count 1.
                /// \param[out] min start.
                /// \param[out] max stop.
                /// \param[out] average stop - start.
                /// \param[out] total stop - start.
                virtual void GetStats (
                    ui32 &count,
                    ui64 &min,
                    ui64 &max,
                    ui64 &average,
                    ui64 &total) const;
                /// \brief
                /// Format an XML string from TimerInfo parameters
                /// and attributes. The string will look like this:
                /// <Timer Name = "name"
                ///        Start = "start"
                ///        Stop = "stop"
                ///        Ellapsed = "stop - start"/>
                /// \param[in] indentationLevel 4 * indentationLevel
                /// spaces to add to the beginning of the string.
                /// \return An XML string from TimerInfo parameters
                /// and attributes.
                virtual std::string ToString (ui32 indentationLevel) const;

                /// \brief
                /// Record the current time as the start time.
                void Start ();
                /// \brief
                /// Record the current time as the stop time.
                void Stop ();
            };
            /// \struct HRTimerMgr::ScopeInfo HRTimerMgr.h thekogans/util/HRTimerMgr.h
            ///
            /// \brief
            /// Hierarchical timer scope. Used to aggregate related
            /// TimerInfo stats.
            struct _LIB_THEKOGANS_UTIL_DECL ScopeInfo : public TimerInfoBase {
                /// \brief
                /// ScopeInfo has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (ScopeInfo, SpinLock)

                /// \brief
                /// "Scope"
                static const char * const TAG_SCOPE;
                /// \brief
                /// "Name"
                static const char * const ATTR_NAME;
                /// \brief
                /// "Count"
                static const char * const ATTR_COUNT;
                /// \brief
                /// "Min"
                static const char * const ATTR_MIN;
                /// \brief
                /// "Max"
                static const char * const ATTR_MAX;
                /// \brief
                /// "Average"
                static const char * const ATTR_AVERAGE;
                /// \brief
                /// "Total"
                static const char * const ATTR_TOTAL;

                /// \brief
                /// A stack of open scopes.
                AbstractOwnerList<TimerInfoBase> open;
                /// \brief
                /// A list of closed scopes.
                AbstractOwnerList<TimerInfoBase> closed;

                /// \brief
                /// ctor.
                /// \param[in] name Name of this ScopeInfo.
                explicit ScopeInfo (const std::string &name) :
                    TimerInfoBase (name) {}

                /// \brief
                /// Return stats accumulated by this ScopeInfo.
                /// \param[out] count Aggregate count of all child scope and timers.
                /// \param[out] min Minimum time spent by a child scope or timer.
                /// \param[out] max Maximum time spent by a child scope or timer.
                /// \param[out] average Average time spent by child scope and timers.
                /// \param[out] total Aggregate time spent in the block.
                virtual void GetStats (
                    ui32 &count,
                    ui64 &min,
                    ui64 &max,
                    ui64 &average,
                    ui64 &total) const;
                /// \brief
                /// Format an XML string from ScopeInfo parameters
                /// and attributes. The string will look like this:
                /// <Scope Name = "name"
                ///        Count = ""
                ///        Min = ""
                ///        Max = ""
                ///        Average = ""
                ///        Total = "">
                ///     sub-scopes and timers.
                /// </Scope>
                /// \param[in] indentationLevel 4 * indentationLevel
                /// spaces to add to the beginning of the string.
                /// \return An XML string from TimerInfo parameters
                /// and attributes.
                virtual std::string ToString (ui32 indentationLevel) const;

                /// \brief
                /// Start a new sub-scope.
                /// \param[in] name Name of the new sub-scope.
                /// \return The new sub-scope.
                ScopeInfo *BeginScope (const std::string &name);
                /// \brief
                /// End the current sub-scope.
                void EndScope ();

                /// \brief
                /// Start a new sub-timer.
                /// \param[in] name Name of the new sub-timer.
                /// \return The new sub-timer.
                TimerInfo *StartTimer (const std::string &name);
                /// \brief
                /// End the current sub-timer.
                void StopTimer ();
            };
            /// \brief
            /// The root of the scope hierarchy.
            ScopeInfo root;
            std::list<ScopeInfo *> scopes;

        public:
            /// \brief
            /// ctor.
            /// \param[in] name Root scope name.
            explicit HRTimerMgr (const std::string &name) :
                root (name) {}
            /// \brief
            /// dtor. Dump the stats to the log.
            ~HRTimerMgr () {
                THEKOGANS_UTIL_LOG_DEBUG (
                    "Profiling results for: %s\n\n%s",
                    root.name.c_str (),
                    ToString (0).c_str ());
            }

            /// \brief
            /// Return root scope.
            /// \return Root scope.
            inline ScopeInfo *GetRootScope () {
                return &root;
            }

            /// \brief
            /// Return current scope.
            /// \return Current scope.
            inline ScopeInfo *GetCurrentScope () {
                return !scopes.empty () ? scopes.back () : &root;
            }

            /// \struct HRTimerMgr::Scope HRTimerMgr.h thekogans/util/HRTimerMgr.h
            ///
            /// \brief
            /// Smart pointer and hierarchy manager for ScopeInfo.
            struct Scope {
                /// \brief
                /// HRTimerMgr to which this scope belongs.
                HRTimerMgr &timerMgr;
                /// \brief
                /// Parent scope.
                ScopeInfo *parent;
                /// \brief
                /// ScopeInfo that this smart pointer represents.
                ScopeInfo *scopeInfo;

                /// \brief
                /// ctor.
                /// \param[in] timerMgr_ HRTimerMgr to which this scope belongs.
                /// \param[in] parent_ Parent scope.
                /// \param[in] name Name of the new scope.
                Scope (HRTimerMgr &timerMgr_,
                        ScopeInfo *parent_,
                        const std::string &name) :
                        timerMgr (timerMgr_),
                        parent (parent_),
                        scopeInfo (parent->BeginScope (name)) {
                    timerMgr.scopes.push_back (scopeInfo);
                }
                /// \brief
                /// dtor. Close the scope, and notify the parent.
                ~Scope () {
                    parent->EndScope ();
                    timerMgr.scopes.pop_back ();
                }

                /// \brief
                /// Add a new attribute to the scope.
                /// \param[in] attribute Attribute to add.
                inline void AddAttribute (const Attribute &attribute) {
                    scopeInfo->AddAttribute (attribute);
                }

                /// \brief
                /// Scope is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Scope)
            };

            /// \brief
            /// Begin a new aggregate scope, and make it current.
            /// \param[in] name Name of new scope.
            /// \return Pointer to the new scope.
            inline ScopeInfo *BeginScope (const std::string &name) {
                return root.BeginScope (name);
            }
            /// \brief
            /// End current scope.
            inline void EndScope () {
                root.EndScope ();
            }

            /// \struct HRTimerMgr::Timer HRTimerMgr.h thekogans/util/HRTimerMgr.h
            ///
            /// \brief
            /// Smart pointer and hierarchy manager for TimerInfo.
            struct Timer {
                /// \brief
                /// HRTimerMgr to which this timer belongs.
                HRTimerMgr &timerMgr;
                /// \brief
                /// Parent scope.
                ScopeInfo *parent;
                /// \brief
                /// TimerInfo that this smart pointer represents.
                TimerInfo *timerInfo;

                /// \brief
                /// ctor.
                /// \param[in] timerMgr_ HRTimerMgr to which this timer belongs.
                /// \param[in] parent_ Parent scope.
                /// \param[in] name Name of the new timer.
                Timer (HRTimerMgr &timerMgr_,
                        ScopeInfo *parent_,
                        const std::string &name) :
                        timerMgr (timerMgr_),
                        parent (parent_),
                        timerInfo (parent->StartTimer (name)) {
                    timerInfo->Start ();
                }
                /// \brief
                /// dtor. Stop the timer, and notify the parent.
                ~Timer () {
                    timerInfo->Stop ();
                    parent->StopTimer ();
                }

                /// \brief
                /// Add a new attribute to the timer.
                /// \param[in] attribute Attribute to add.
                inline void AddAttribute (const Attribute &attribute) {
                    timerInfo->AddAttribute (attribute);
                }

                /// \brief
                /// Timer is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Timer)
            };

            /// \brief
            /// Start a new timer, and make it current.
            /// \param[in] name Name of the new timed section.
            /// \return Pointer to the new timer.
            inline TimerInfo *StartTimer (const std::string &name) {
                return root.StartTimer (name);
            }
            /// \brief
            /// Stop the current timer.
            inline void StopTimer () {
                root.StopTimer ();
            }

            /// \brief
            /// Aggregate and return the current stats.
            /// \param[out] count Aggregate number of times this block was hit.
            /// \param[out] min Aggregate minimum time spent in the block.
            /// \param[out] max Aggregate maximum time spent in the block.
            /// \param[out] average Aggregate average time spent in the block.
            /// \param[out] total Aggregate total time spent in the block.
            inline void GetStats (
                    ui32 &count,
                    ui64 &min,
                    ui64 &max,
                    ui64 &average,
                    ui64 &total) const {
                count = 0;
                min = UI64_MAX;
                max = average = total = 0;
                root.GetStats (count, min, max, average, total);
            }

            /// \brief
            /// Add a user defind attribute to the root scope.
            /// \param[in] attribute Used defined key/value pair.
            inline void AddAttribute (const Attribute &attribute) {
                root.AddAttribute (attribute);
            }

            /// \brief
            /// Format an XML string from root scope parameters
            /// and attributes. The string will look like this:
            /// <Scope Name = "name"
            ///        Count = ""
            ///        Min = ""
            ///        Max = ""
            ///        Average = ""
            ///        Total = "">
            ///     sub-scopes and timers.
            /// </Scope>
            /// \param[in] indentationLevel 4 * indentationLevel
            /// spaces to add to the beginning of the string.
            /// \return An XML string from root scope parameters
            /// and attributes.
            inline std::string ToString (ui32 indentationLevel) const {
                return root.ToString (indentationLevel);
            }

            /// \brief
            /// HRTimerMgr is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (HRTimerMgr)
        };

        #if defined (THEKOGANS_UTIL_USE_HRTIMER_MGR)
            /// \def THEKOGANS_UTIL_HRTIMER_MGR(name, ...)
            /// Declare an instance of HRTimerMgr
            /// with a given name.
            /// \param[in] name Name used to label
            /// the report produced by this HRTimerMgr.
            #define THEKOGANS_UTIL_HRTIMER_MGR(name, ...)\
                thekogans::util::HRTimerMgr timerMgr (\
                    thekogans::util::FormatString (name, __VA_ARGS__))
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_ADD_ATTRIBUTE(attribute)
            /// Add an attribute (name, value) to
            /// the HRTimerMgr currently in scope.
            /// Attributes allow you to add custom
            /// info to reports.
            /// \param[in] attribute Attribute to
            /// add to the current HRTimerMgr.
            #define THEKOGANS_UTIL_HRTIMER_MGR_ADD_ATTRIBUTE(attribute)\
                timerMgr.AddAttribute (attribute)
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_SCOPE(name, ...)
            /// Start a new scope for the current HRTimerMgr.
            /// Scopes are nested, and aggregate their
            /// results up the hierarchy.
            /// \param[in] name Name of the scope.
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE(name, ...)\
                thekogans::util::HRTimerMgr::Scope scope (\
                    timerMgr, timerMgr.GetRootScope (),\
                    thekogans::util::FormatString (name, __VA_ARGS__))
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_ADD_ATTRIBUTE(attribute)
            /// Add an attribute to the current scope.
            /// \param[in] attribute Attribute to
            /// add to the current scope.
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_ADD_ATTRIBUTE(attribute)\
                scope.AddAttribute (attribute)
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_TIMER(name, ...)
            /// Create a new HRTimerMgr timer.
            /// \param[in] name Name of the new timer.
            #define THEKOGANS_UTIL_HRTIMER_MGR_TIMER(name, ...)\
                thekogans::util::HRTimerMgr::Timer timer (\
                    timerMgr, timerMgr.GetRootScope (),\
                    thekogans::util::FormatString (name, __VA_ARGS__))
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_TIMER_ADD_ATTRIBUTE(attribute)
            /// Add an attribute to the current timer.
            /// \param[in] attribute Attribute to
            /// add to the current timer.
            #define THEKOGANS_UTIL_HRTIMER_MGR_TIMER_ADD_ATTRIBUTE(attribute)\
                timer.AddAttribute (attribute)
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_SCOPE(name, ...)
            /// Start a new sub-scope for the current scope.
            /// Scopes are nested, and aggregate their
            /// results up the hierarchy.
            /// \param[in] name Name of the sub-scope.
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_SCOPE(name, ...)\
                thekogans::util::HRTimerMgr::Scope scope (\
                    timerMgr, timerMgr.GetCurrentScope (),\
                    thekogans::util::FormatString (name, __VA_ARGS__))
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER(name, ...)
            /// Create a new scope timer.
            /// \param[in] name Name of the new timer.
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER(name, ...)\
                thekogans::util::HRTimerMgr::Timer timer (\
                    timerMgr, timerMgr.GetCurrentScope (),\
                    thekogans::util::FormatString (name, __VA_ARGS__))
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_PROTO
            /// Use this macro in function prototypes that
            /// take one parameter, namely the HRTimerMgr
            /// reference. Or where HRTimerMgr is the last
            /// parameter.
            #define THEKOGANS_UTIL_HRTIMER_MGR_PROTO thekogans::util::HRTimerMgr &timerMgr
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_GET
            /// Use this macro in function call sites that
            /// take only one parameter, namely HRTimerMgr.
            /// Or where HRTimerMgr is the last parameter.
            #define THEKOGANS_UTIL_HRTIMER_MGR_GET timerMgr
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_PROTO_COMMA
            /// Use this macro in function prototypes that
            /// take multiple parameters, and a comma is
            /// required after HRTimerMgr.
            #define THEKOGANS_UTIL_HRTIMER_MGR_PROTO_COMMA thekogans::util::HRTimerMgr &timerMgr,
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_GET_COMMA
            /// Use this macro in function call sites that
            /// take multiple parameters and a comma is
            /// required after HRTimerMgr.
            #define THEKOGANS_UTIL_HRTIMER_MGR_GET_COMMA timerMgr,
            /// \def THEKOGANS_UTIL_HRTIMER_MGR_LOG(level)
            /// Dump the state of the HRTimerMgr currently
            /// in scope to the log.
            /// \param[in] level Log level for the report.
            #define THEKOGANS_UTIL_HRTIMER_MGR_LOG(level)\
                THEKOGANS_UTIL_LOG (level, "Profiling results for: %s\n\n%s",\
                    timerMgr.GetRootScope ().name.c_str (), timerMgr.ToString (0).c_str ())
        #else // defined (THEKOGANS_UTIL_USE_HRTIMER_MGR)
            #define THEKOGANS_UTIL_HRTIMER_MGR(name, ...)
            #define THEKOGANS_UTIL_HRTIMER_MGR_ADD_ATTRIBUTE(attribute)
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE(name, ...)
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_ADD_ATTRIBUTE(attribute)
            #define THEKOGANS_UTIL_HRTIMER_MGR_TIMER(name, ...)
            #define THEKOGANS_UTIL_HRTIMER_MGR_TIMER_ADD_ATTRIBUTE(attribute)
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_SCOPE(name, ...)
            #define THEKOGANS_UTIL_HRTIMER_MGR_SCOPE_TIMER(name, ...)
            #define THEKOGANS_UTIL_HRTIMER_MGR_PROTO
            #define THEKOGANS_UTIL_HRTIMER_MGR_GET
            #define THEKOGANS_UTIL_HRTIMER_MGR_PROTO_COMMA
            #define THEKOGANS_UTIL_HRTIMER_MGR_GET_COMMA
            #define THEKOGANS_UTIL_HRTIMER_MGR_LOG(level)
        #endif // defined (THEKOGANS_UTIL_USE_HRTIMER_MGR)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_HRTimerMgr_h)
