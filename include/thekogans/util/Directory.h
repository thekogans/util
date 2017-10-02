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

#if !defined (__thekogans_util_Directory_h)
#define __thekogans_util_Directory_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#else // defined (TOOLCHAIN_OS_Windows)
    #include <sys/stat.h>
    #include <dirent.h>
#if defined (TOOLCHAIN_OS_Linux)
    #include <sys/inotify.h>
    #include <sys/select.h>
#endif // defined (TOOLCHAIN_OS_Linux)
#endif // defined (TOOLCHAIN_OS_Windows)
#include <cctype>
#include <string>
#if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
    #include <pugixml.hpp>
#endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Heap.h"
#include "thekogans/util/OwnerMap.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Serializer.h"

namespace thekogans {
    namespace util {

        /// \struct Directory Directory.h thekogans/util/Directory.h
        ///
        /// \brief
        /// Directory is a platform independent file system directory
        /// traversal and change notification class.
        /// Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// thekogans::util::Directory directory (path);
        /// thekogans::util::Directory::Entry entry;
        /// for (bool gotEntry = directory.GetFirstEntry (entry);
        ///         gotEntry; gotEntry = directory.GetNextEntry (entry)) {
        ///     if (!entry.name.empty ()) {
        ///         if (entry.type == thekogans::util::Directory::Entry::Folder) {
        ///             if (!thekogans::util::IsDotOrDotDot (entry.name.c_str ())) {
        ///                 do something with a directory entry.
        ///             }
        ///         }
        ///         else if (entry.type == thekogans::util::Directory::Entry::File ||
        ///                 entry.type == thekogans::util::Directory::Entry::Link) {
        ///             do something with a file/link entry.
        ///         }
        ///     }
        ///     else {
        ///         THEKOGANS_UTIL_LOG_ERROR ("Malformed directory entry: '%s'\n",
        ///             entry.ToString (0).c_str ());
        ///     }
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL Directory {
            struct Entry;

            /// \struct Directory::Watcher Directory.h thekogans/util/Directory.h
            ///
            /// \brief
            /// Watcher is a platform independent directory
            /// change notification singleton. It will watch
            /// a requested directory for changes and notify
            /// the callback when something interesting happens.
            /// Watcher is NOT recursive. If you want to watch
            /// all directories in a given branch, do something
            /// like this:
            ///
            /// \code{.cpp}
            /// void WatchBranch (const std::string &path,
            ///         thekogans::util::Directory::Watcher::EventSink &evenSink,
            ///         std::list<thekogans::util::Directory::Watcher::WatchId> &watches) {
            ///     thekogans::util::Directory::Watcher::WatchId watchId =
            ///         thekogans::util::Directory::Watcher::Instance ().AddWatch (
            ///             path, evenSink);
            ///     assert (watchId != THEKOGANS_UTIL_INVALID_HANDLE_VALUE);
            ///     watches.push_back (watchId);
            ///     thekogans::util::Directory directory (path);
            ///     thekogans::util::Directory::Entry entry;
            ///     for (bool gotEntry = directory.GetFirstEntry (entry);
            ///             gotEntry; gotEntry = directory.GetNextEntry (entry)) {
            ///         if (entry.type == thekogans::util::Directory::Entry::Folder &&
            ///                 !thekogans::util::IsDotOrDotDot (entry.name.c_str ())) {
            ///             WatchBranch (thekogans::util::MakePath (path, entry.name),
            ///                 evenSink, watches);
            ///         }
            ///     }
            /// }
            /// \endcode
            ///
            /// Here is a canonical use case:
            ///
            /// \code{.cpp}
            /// struct Options :
            ///         public thekogans::util::Singleton<Options>,
            ///         public thekogans::util::CommandLineOptions,
            ///         public thekogans::util::Directory::Watcher::EventSink {
            ///     ...
            ///     std::string startDirectory;
            ///     thekogans::util::Directory::Watcher::WatchId watchId;
            ///     ...
            ///     Options () :
            ///             startDirectory (thekogans::util::Path::GetCurrPath ()),
            ///             watchId (
            ///                 thekogans::util::Directory::Watcher::Instance ().AddWatch (
            ///                     startDirectory, *this)) {
            ///         if (thekogans::util::Path (
            ///                 thekogans::util::MakePath (
            ///                     startDirectory, OPTIONS_XML)).Exists ()) {
            ///             ReadConfig ();
            ///         }
            ///     }
            ///     ...
            ///     // thekogans::util::Directory::Watcher::EventSink
            ///     virtual void HandleModified (
            ///             thekogans::util::Directory::Watcher::WatchId watchId_,
            ///             const std::string &directory,
            ///             const thekogans::util::Directory::Entry &entry) {
            ///         if (watchId_ == watchId && directory == startDirectory &&
            ///                 entry.name == OPTIONS_XML) {
            ///             ReadConfig ();
            ///         }
            ///     }
            ///     ...
            /// };
            /// \endcode
            ///
            /// Note how Options is both a thekogans::util::CommandLineOptions,
            /// and a thekogans::util::Directory::Watcher::EventSink derivative.
            /// This is on purpose to allow the app to both read it's options from
            /// a file (which might be updated at run-time), and have some/all of
            /// those options overiden on the command line.
            struct _LIB_THEKOGANS_UTIL_DECL Watcher :
                    public Singleton<Watcher, SpinLock>,
                    public Thread {
                /// \brief
                /// Convenient typedef for THEKOGANS_UTIL_HANDLE.
                typedef THEKOGANS_UTIL_HANDLE WatchId;

                /// \struct Directory::Watcher::EventSink Directory.h thekogans/util/Directory.h
                ///
                /// \brief
                /// EventSink specifies the interface used
                /// by the Watcher to send change notifications.
                struct _LIB_THEKOGANS_UTIL_DECL EventSink {
                    /// \brief
                    /// dtor.
                    virtual ~EventSink () {}

                    /// \brief
                    /// Called when an error occurs.
                    /// \param[in] watchId Watch id of the affected directory.
                    /// \param[in] directory Path of the affected directory.
                    /// \param[in] exception Exception representing the error.
                    virtual void HandleError (
                        WatchId watchId,
                        const std::string &directory,
                        const Exception &exception) {}
                    /// \brief
                    /// Called when a new entry was added to
                    /// the watched directory.
                    /// \param[in] watchId Watch id of the affected directory.
                    /// \param[in] directory Path of the affected directory.
                    /// \param[in] entry The new entry.
                    virtual void HandleAdd (
                        WatchId watchId,
                        const std::string &directory,
                        const Directory::Entry &entry) {}
                    /// \brief
                    /// Called when an entry was deleted from
                    /// the watched directory.
                    /// \param[in] watchId Watch id of the affected directory.
                    /// \param[in] directory Path of the affected directory.
                    /// \param[in] entry The deleted entry.
                    virtual void HandleDelete (
                        WatchId watchId,
                        const std::string &directory,
                        const Directory::Entry &entry) {}
                    /// \brief
                    /// Called when an entry was modified.
                    /// \param[in] watchId Watch id of the affected directory.
                    /// \param[in] directory Path of the affected directory.
                    /// \param[in] entry The modified entry.
                    virtual void HandleModified (
                        WatchId watchId,
                        const std::string &directory,
                        const Directory::Entry &entry) {}
                };

                struct Watch;

            private:
                /// \brief
                /// OS specific Watcher handle.
                THEKOGANS_UTIL_HANDLE handle;
            #if defined (TOOLCHAIN_OS_Linux)
                /// \brief
                /// Handle to epoll queue that will listen for async events.
                THEKOGANS_UTIL_HANDLE epollHandle;
            #endif // defined (TOOLCHAIN_OS_Linux)
                /// \brief
                /// typedef for OwnerMap<WatchId, Watch>.
                typedef OwnerMap<WatchId, Watch> Watches;
                /// \brief
                /// Current watches.
                Watches watches;
                /// \brief
                /// Synchronization spin lock.
                SpinLock spinLock;

            public:
                /// \brief
                /// Default ctor.
                Watcher ();
                /// \brief
                /// dtor.
                /// This is just for show. Watcher is a Singleton.
                ~Watcher ();

                /// \brief
                /// Add a directory to watch for changes.
                /// \param[in] directory Directory to watch.
                /// \param[in] evenSink Event sink to notify of changes.
                /// \return Newly added watch id.
                WatchId AddWatch (
                    const std::string &directory,
                    EventSink &evenSink);
                /// \brief
                /// Given a watch id, return associated directory.
                /// \param[in] watchId A watch id returned by AddWatch.
                /// \return If watch id is registered, associated directory,
                /// empty string otherwise.
                std::string GetDirectory (WatchId watchId);
                /// \brief
                /// Remove a previously added watch.
                /// \param[in] watchId A watch id returned by AddWatch.
                void RemoveWatch (WatchId watchId);

            protected:
                // Thread
                /// \brief
                /// Thread override to listen for and dispatch
                /// change notifications.
                virtual void Run () throw ();
            };

            /// \brief
            /// Directory path.
            const std::string path;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows directory traversal handle.
            THEKOGANS_UTIL_HANDLE handle;
            /// \brief
            /// Windows directory attributes.
            ui32 attributes;
            /// \brief
            /// Windows directory creation date and time.
            i64 creationDate;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX directory traversal handle.
            DIR *dir;
            /// \brief
            /// Permission flags.
            i32 mode;
            /// \brief
            /// POSIX directory last status date and time.
            i64 lastStatusDate;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Directory last accessed date and time.
            i64 lastAccessedDate;
            /// \brief
            /// Directory last modified date and time.
            i64 lastModifiedDate;

            /// \brief
            /// ctor.
            /// \param[in] path_ The path this Directory object represents.
            explicit Directory (const std::string &path_);
            /// \brief
            /// dtor.
            ~Directory ();

            /// \struct Directory::Entry Directory.h thekogans/util/Directory.h
            ///
            /// \brief
            /// Represents a directory entry.
            struct _LIB_THEKOGANS_UTIL_DECL Entry {
                /// \brief
                /// "Entry"
                static const char * const TAG_ENTRY;
                /// \brief
                /// "FileSystem"
                static const char * const ATTR_FILE_SYSTEM;
                /// \brief
                /// "Windows"
                static const char * const VALUE_WINDOWS;
                /// \brief
                /// "POSIX"
                static const char * const VALUE_POSIX;
                /// \brief
                /// "Type"
                static const char * const ATTR_TYPE;
                /// \brief
                /// "invalid"
                static const char * const VALUE_INVALID;
                /// \brief
                /// "file"
                static const char * const VALUE_FILE;
                /// \brief
                /// "folder"
                static const char * const VALUE_FOLDER;
                /// \brief
                /// "link"
                static const char * const VALUE_LINK;
                /// \brief
                /// "Name"
                static const char * const ATTR_NAME;
                /// \brief
                /// "Attributes"
                static const char * const ATTR_ATTRIBUTES;
                /// \brief
                /// "CreationDate"
                static const char * const ATTR_CREATION_DATE;
                /// \brief
                /// "Mode"
                static const char * const ATTR_MODE;
                /// \brief
                /// "LastStatusDate"
                static const char * const ATTR_LAST_STATUS_DATE;
                /// \brief
                /// "LastAccessedDate"
                static const char * const ATTR_LAST_ACCESSED_DATE;
                /// \brief
                /// "LastModifiedDate"
                static const char * const ATTR_LAST_MODIFIED_DATE;
                /// \brief
                /// "Size"
                static const char * const ATTR_SIZE;

                /// \brief
                /// The file system this entry came from.
                enum {
                    /// \brief
                    /// Windows file system.
                    Windows,
                    /// \brief
                    /// POSIX (Linux/OS X) file system.
                    POSIX
                };
                ui32 fileSystem;
                /// \brief
                /// Entry type.
                enum {
                    /// \brief
                    /// Invalid entry.
                    Invalid,
                    /// \brief
                    /// Entry is a regular file.
                    File,
                    /// \brief
                    /// Entry is a directory.
                    Folder,
                    /// \brief
                    /// Entry is a symbolic link.
                    Link
                };
                /// \brief
                /// Entry type.
                i32 type;
                /// \brief
                /// Entry name.
                std::string name;
                union {
                    /// \brief
                    /// Windows entry attributes,
                    ui32 attributes;
                    /// \brief
                    /// POSIX entry permission flags.
                    i32 mode;
                };
                union {
                    /// \brief
                    /// Windows entry creation date.
                    i64 creationDate;
                    /// \brief
                    /// POSIX entry last status date.
                    i64 lastStatusDate;
                };
                /// \brief
                /// Entry last accessed date.
                i64 lastAccessedDate;
                /// \brief
                /// Entry last modified date.
                i64 lastModifiedDate;
                /// \brief
                /// File size.
                ui64 size;

                /// \brief
                /// Default ctor.
                Entry () :
                #if defined (TOOLCHAIN_OS_Windows)
                    fileSystem (Windows),
                #else // defined (TOOLCHAIN_OS_Windows)
                    fileSystem (POSIX),
                #endif // defined (TOOLCHAIN_OS_Windows)
                    type (Invalid),
                #if defined (TOOLCHAIN_OS_Windows)
                    attributes (0),
                    creationDate (-1),
                #else // defined (TOOLCHAIN_OS_Windows)
                    mode (0),
                    lastStatusDate (-1),
                #endif // defined (TOOLCHAIN_OS_Windows)
                    lastAccessedDate (-1),
                    lastModifiedDate (-1),
                    size (0) {}
                /// \brief
                /// ctor. Read entry info from file system.
                /// \param[in] path File system path to get entry info from.
                explicit Entry (const std::string &path);

                /// \brief
                /// Empty entry. Use this const to compare against empty entries.
                static const Entry Empty;

                /// \brief
                /// Given a numeric fileSystem, return a string representation.
                /// \param[in] fileSystem Numeric fileSystem (Windows/POSIX)
                /// \return "Windows" | "POSIX"
                static std::string fileSystemTostring (ui32 fileSystem);
                /// \brief
                /// Given a string fileSystem, return a numeric representation.
                /// \param[in] fileSystem String fileSystem ("Windows" | "POSIX")
                /// \return Invalid/File/Folder/Link
                static ui32 stringTofileSystem (const std::string &fileSystem);

                /// \brief
                /// Given a numeric type, return a string representation.
                /// \param[in] type Numeric type (Invalid/File/Folder/Link)
                /// \return "Invalid" | "File" | "Folder" | "Link"
                static std::string typeTostring (i32 type);
                /// \brief
                /// Given a string type, return a numeric representation.
                /// \param[in] type String type ("Invalid" | "File" | "Folder" |"Link")
                /// \return Invalid/File/Folder/Link
                static i32 stringTotype (const std::string &type);

                /// \brief
                /// Return the serialized size of this entry.
                /// \return Serialized size of this entry.
                inline ui32 Size () const {
                    return
                        Serializer::Size (fileSystem) +
                        Serializer::Size (type) +
                        Serializer::Size (name) +
                        (fileSystem == Windows ?
                            Serializer::Size (attributes) +
                            Serializer::Size (creationDate) :
                            Serializer::Size (mode) +
                            Serializer::Size (lastStatusDate)) +
                        Serializer::Size (lastAccessedDate) +
                        Serializer::Size (lastModifiedDate) +
                        Serializer::Size (size);
                }

                /// \brief
                /// Compare two entries irrespective of lastAccessedDate.
                /// This timestamp is updated on every access (including reads),
                /// and doesn't give a true measure of difference.
                /// \param[in] entry Entry to compare to.
                /// \return true = equal, false = different
                inline bool CompareWeakly (const Entry &entry) const {
                    return
                        fileSystem == entry.fileSystem &&
                        type == entry.type &&
                        name == entry.name &&
                        (fileSystem == entry.fileSystem ?
                            attributes == entry.attributes &&
                            creationDate == entry.creationDate :
                            mode == entry.mode &&
                            lastStatusDate == entry.lastStatusDate) &&
                        lastModifiedDate == entry.lastModifiedDate &&
                        size == entry.size;
                }

            #if defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                /// \brief
                /// Parse entry info from an xml dom that looks like this;
                /// <Entry FileSystem = "Windows | POSIX"
                ///        Type = "Invalid | File | Folder | Link"
                ///        Name = ""
                ///    if (fileSystem == Windows) {
                ///        Attributes = ""
                ///        CreationDate = ""
                ///    }
                ///    else {
                ///        Mode = ""
                ///        LastStatusDate = ""
                ///    }
                ///        LastAccessedDate = ""
                ///        LastModifiedDate = ""
                ///        Size = ""/>
                /// \param[in] node DOM representation of an entry.
                void Parse (const pugi::xml_node &node);
            #endif // defined (THEKOGANS_UTIL_HAVE_PUGIXML)
                /// \brief
                /// Encode the entry as an xml string.
                /// Take care to encode all xml char entities properly.
                /// \param[in] indentationLevel Number of '\t' to preceed the entry with.
                /// \return encoded xml string.
                std::string ToString (
                    ui32 indentationLevel,
                    const char *tagName = TAG_ENTRY) const;
            };

            /// \brief
            /// Return the first entry in the directory.
            /// \param[out] entry Where the first entry info will be placed.
            /// \return true = got entry, false = directory is empty.
            bool GetFirstEntry (Entry &entry);
            /// \brief
            /// Return the next entry in the directory.
            /// \param[out] entry Where the next entry info will be placed.
            /// \return true = got entry, false = no more entries.
            bool GetNextEntry (Entry &entry);

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Create a new directory, optionally creating it's ancestry.
            /// \param[in] path New directory path.
            /// \param[in] createAncestry true = create encestry (if it does not exist),
            /// false = create only the final directory.
            /// \param[in] lpSecurityAttributes Windows security
            /// atributes of the new directory.
            static void Create (
                const std::string &path,
                bool createAncestry = true,
                LPSECURITY_ATTRIBUTES lpSecurityAttributes = 0);
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Create a new directory, optionally creating it's ancestry.
            /// \param[in] path New directory path
            /// \param[in] createAncestry true = create encestry (if it does not exist),
            /// false = create only the final directory.
            /// \param[in] mode UNIX permission flags.
            static void Create (
                const std::string &path,
                bool createAncestry = true,
                mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Delete an existing directory, optionally deleting
            /// the branch rooted at the given directory.
            /// \param[in] path Directory path to delete.
            /// \param[in] recursive true = delete branch,
            /// false = delete only the specified directory.
            static void Delete (
                const std::string &path,
                bool recursive = true);

        private:
            /// \brief
            /// Cloase the directory traversal handles.
            void Close ();
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows directory traversal get entry.
            /// \param[in] findData Windows FindFirstFile/FindNextFile data.
            /// \param[out] entry Entry info.
            void GetEntry (
                const WIN32_FIND_DATA &findData,
                Entry &entry) const;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// POSIX directory traversal get entry.
            /// \param[in] dirEnt POSIX opendir data.
            /// \param[out] entry Entry info.
            void GetEntry (
                const dirent &dirEnt,
                Entry &entry) const;
        #endif // defined (TOOLCHAIN_OS_Windows)
        };

        /// \brief
        /// Compare two directory entries for equality.
        /// \param[in] entry1 First entry to compare.
        /// \param[in] entry2 Second entry to compare.
        /// \return true = equal, false = different.
        inline bool operator == (
                const Directory::Entry &entry1,
                const Directory::Entry &entry2) {
            return
                entry1.fileSystem == entry2.fileSystem &&
                entry1.type == entry2.type &&
                entry1.name == entry2.name &&
                (entry1.fileSystem == Directory::Entry::Windows ?
                    entry1.attributes == entry2.attributes &&
                    entry1.creationDate == entry2.creationDate :
                    entry1.mode == entry2.mode &&
                    entry1.lastStatusDate == entry2.lastStatusDate) &&
                entry1.lastAccessedDate == entry2.lastAccessedDate &&
                entry1.lastModifiedDate == entry2.lastModifiedDate &&
                entry1.size == entry2.size;
        }

        /// \brief
        /// Compare two directory entries for inequality.
        /// \param[in] entry1 First entry to compare.
        /// \param[in] entry2 Second entry to compare.
        /// \return true = different, false = equal.
        inline bool operator != (
                const Directory::Entry &entry1,
                const Directory::Entry &entry2) {
            return
                entry1.fileSystem != entry2.fileSystem ||
                entry1.type != entry2.type ||
                entry1.name != entry2.name ||
                (entry1.fileSystem == Directory::Entry::Windows ?
                    entry1.attributes != entry2.attributes ||
                    entry1.creationDate != entry2.creationDate :
                    entry1.mode != entry2.mode ||
                    entry1.lastStatusDate != entry2.lastStatusDate) ||
                entry1.lastAccessedDate != entry2.lastAccessedDate ||
                entry1.lastModifiedDate != entry2.lastModifiedDate ||
                entry1.size != entry2.size;
        }

        /// \brief
        /// Write the given entry to the given serializer.
        /// \param[in] serializer Where to write the given entry.
        /// \param[in] entry \see{Directory::Entry} to write.
        /// \return serializer.
        inline Serializer &operator << (
                Serializer &serializer,
                const Directory::Entry &entry) {
            serializer <<
                entry.fileSystem <<
                entry.type <<
                entry.name;
            if (entry.fileSystem == Directory::Entry::Windows) {
                serializer <<
                    entry.attributes <<
                    entry.creationDate;
            }
            else {
                serializer <<
                    entry.mode <<
                    entry.lastStatusDate;
            }
            serializer <<
                entry.lastAccessedDate <<
                entry.lastModifiedDate <<
                entry.size;
            return serializer;
        }

        /// \brief
        /// Read an entry from the given serializer.
        /// \param[in] serializer Where to read the entry from.
        /// \param[in] entry \see{Directory::Entry} to read.
        /// \return serializer.
        inline Serializer &operator >> (
                Serializer &serializer,
                Directory::Entry &entry) {
            serializer >>
                entry.fileSystem >>
                entry.type >>
                entry.name;
            if (entry.fileSystem == Directory::Entry::Windows) {
                serializer >>
                    entry.attributes >>
                    entry.creationDate;
            }
            else {
                serializer >>
                    entry.mode >>
                    entry.lastStatusDate;
            }
            serializer >>
                entry.lastAccessedDate >>
                entry.lastModifiedDate >>
                entry.size;
            return serializer;
        }

        /// \brief
        /// Return true if name points to '.' or '..'
        inline bool IsDotOrDotDot (const char *name) {
            return name != 0 && name[0] == '.' &&
                (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
        }

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Directory_h)
