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

#if !defined (__thekogans_util_Plugins_h)
#define __thekogans_util_Plugins_h

#include <memory>
#include <string>
#include <set>
#include <map>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Version.h"
#include "thekogans/util/DynamicLibrary.h"

namespace thekogans {
    namespace util {

        /// \struct Plugins Plugins.h thekogans/util/Plugins.h
        ///
        /// \brief
        /// Plugins manages dynamically loadable modules (plugins) for a program or library.
        /// The plugins for a module are discribed in an xml file with the following structure.
        ///
        /// <plugins schema_version = "1">
        ///   <plugin path = "plugin path relative to the xml file"
        ///           version = "expected plugin version"
        ///           SHA2-256 = "plugin signature used for integrity checks">
        ///     <dependencies>
        ///       <dependency>path</dependency>
        ///       ...
        ///     </dependencies>
        ///   </plugin>
        ///   ...
        /// </plugins>

        struct _LIB_THEKOGANS_UTIL_DECL Plugins {
            /// \struct Plugins::Plugin Plugins.h thekogans/util/Plugins.h
            ///
            /// \brief
            /// Represents a plugin found in the xml file.
            struct _LIB_THEKOGANS_UTIL_DECL Plugin : public virtual RefCounted {
                /// \brief
                /// Convenient typedef for RefCounted::SharedPtr<Plugin>.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Plugin)

                /// \brief
                /// Plugin has a private heap to help with memory
                /// management, performance, and global heap fragmentation.
                THEKOGANS_UTIL_DECLARE_STD_ALLOCATOR_FUNCTIONS

                /// \brief
                /// Plugin path relative to the xml file.
                std::string path;
                /// \brief
                /// Expected plugin version.
                std::string version;
                /// \brief
                /// Plugin signature used for integrity checks.
                std::string SHA2_256;
                /// \brief
                /// Convenient typedef for std::set<std::string>.
                typedef std::set<std::string> Dependencies;
                /// \brief
                /// Plugin dependencies.
                Dependencies dependencies;
                /// \brief
                /// Loaded plugin.
                DynamicLibrary dynamicLibrary;

                /// \struct Plugins::Plugin::Interface Plugins.h thekogans/util/Plugins.h
                ///
                /// \brief
                /// Every plugin must implement this struct and provide an exportable function
                /// called GetPluginInterface to retrieve it on demand. Here's a canonical use case:
                ///
                /// \code{.cpp}
                /// using namespace thekogans;
                ///
                /// extern "C" WINDOWS_EXPORT_IMPORT util::Plugins::Plugin::Interface &
                /// _LIB_THEKOGANS_UTIL_API GetPluginInterface () {
                ///     struct PluginInterface : public util::Plugins::Plugin::Interface {
                ///         ...
                ///     } static pluginInterface;
                ///     return pluginInterface;
                /// }
                /// \endcode
                struct _LIB_THEKOGANS_UTIL_DECL Interface {
                    /// \brief
                    /// dtor.
                    virtual ~Interface () {}

                    /// \brief
                    /// Return plugin version.
                    /// \return Plugin version.
                    virtual const Version &GetVersion () const throw () = 0;

                    /// \brief
                    /// Called after loading to allow the plugin to initialize itself.
                    virtual void Initialize () throw () {}
                    /// \brief
                    /// Called before unloading to allow the plugin to clean up after itself.
                    virtual void Shutdown () throw () {}
                };

                /// \brief
                /// Convenient typedef for Interface & (_LIB_THEKOGANS_UTIL_API *) ().
                typedef Interface & (_LIB_THEKOGANS_UTIL_API *GetPluginInterfaceProc) ();

                /// \brief
                /// ctor.
                /// \param[in] path_ Plugin path relative to the xml file.
                /// \param[in] version_ Expected plugin version.
                /// \param[in] SHA2_256_ Plugin signature used for integrity checks.
                /// \param[in] dependencies_ Plugin dependencies.
                Plugin (
                    const std::string &path_,
                    const std::string &version_,
                    const std::string &SHA2_256_,
                    const Dependencies &dependencies_ = Dependencies ()) :
                    path (path_),
                    version (version_),
                    SHA2_256 (SHA2_256_),
                    dependencies (dependencies_) {}

                /// \brief
                /// Load the shared library associated with the plugin.
                /// \param[in] directory Directory where the xml file resides.
                void Load (const std::string &directory);
                /// \brief
                /// Unload the shared library associated with the plugin.
                void Unload ();

                /// \brief
                /// Plugin is neither copy constructable, nor assignable.
                THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Plugin)
            };

            /// \brief
            /// Convenient typedef std::map<std::string, Plugin::SharedPtr>.
            typedef std::map<std::string, Plugin::SharedPtr> PluginMap;

        private:
            /// \brief
            /// Plugins xml file path.
            std::string path;
            /// \brief
            /// Map of plugins found in the xml file.
            PluginMap plugins;
            /// \brief
            /// true = the structure has been modified.
            bool modified;

        public:
            enum {
                /// \brief
                /// Default max plugins file size.
                DEFAULT_MAX_PLUGINS_FILE_SIZE = 1024 * 1024
            };

            /// \brief
            /// ctor.
            /// \param[in] path_ Path to the plugins xml file.
            /// \param[in] maxPluginsFileSize Check the file size
            /// and throw if bigger then maxPluginsFileSize.
            Plugins (
                const std::string &path_,
                std::size_t maxPluginsFileSize = DEFAULT_MAX_PLUGINS_FILE_SIZE);

            /// \brief
            /// Return plugin map.
            /// \return Plugin map.
            inline const PluginMap &GetPluginMap () const {
                return plugins;
            }
            /// \brief
            /// Return the plugin with the specified path.
            /// \param[in] path Plugin::path to return.
            /// \return Plugin whose path == path.
            Plugin::SharedPtr GetPlugin (const std::string &path) const;
            /// \brief
            /// Add a plugin to the map. If a plugin containing the path already
            /// exists, update version and SHA2_256 if different.
            /// \param[in] path Plugin path relative to the xml file.
            /// \param[in] version Expected plugin version.
            /// \param[in] SHA2_256 Plugin signature used for integrity checks.
            /// \param[in] dependencies Plugin dependencies.
            void AddPlugin (
                const std::string &path,
                const std::string &version,
                const std::string &SHA2_256,
                const Plugin::Dependencies &dependencies = Plugin::Dependencies ());
            /// \brief
            /// Delete the plugin identified by the given path.
            /// \param[in] path Path of the plugin to delete.
            void DeletePlugin (const std::string &path);
            /// \brief
            /// Delete all plugins.
            void DeletePlugins ();

            /// \brief
            /// Save the plugin map to the file.
            void Save ();

            /// \brief
            /// Load all plugins associated with this xml file.
            void Load ();
            /// \brief
            /// Unload all plugins associated with this xml file.
            void Unload ();

        private:
            /// \brief
            /// Parse the plugins tag.
            /// \param[in] node Root node.
            void ParsePlugins (pugi::xml_node &node);
            /// \brief
            /// Parse a plugin tag.
            /// \param[in] node Node associated with the plugin.
            void ParsePlugin (pugi::xml_node &node);
            /// \brief
            /// Parse plugin dependencies tag.
            /// \param[in] node Node associated with plugin dependencies.
            /// \param[in] plugin Plugin that will receive the parsed dependencies.
            void ParseDependencies (
                pugi::xml_node &node,
                Plugin &plugin);

            /// \brief
            /// Plugins is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Plugins)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Plugins_h)
