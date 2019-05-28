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

#include <fstream>
#include "thekogans/util/Exception.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/File.h"
#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/SHA2.h"
#include "thekogans/util/Plugins.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (Plugins::Plugin, SpinLock)

        void Plugins::Plugin::Load (const std::string &directory) {
            std::string pluginPath = MakePath (directory, path);
            Hash::Digest digest;
            SHA2 hasher;
            hasher.FromFile (pluginPath, SHA2::DIGEST_SIZE_256, digest);
            if (Hash::DigestTostring (digest) == SHA2_256) {
                dynamicLibrary.Load (pluginPath);
                GetPluginInterfaceProc GetPluginInterface =
                    (GetPluginInterfaceProc)dynamicLibrary.GetProc ("GetPluginInterface");
                if (GetPluginInterface != 0) {
                    Interface &interface = GetPluginInterface ();
                    std::string pluginVersion = interface.GetVersion ().ToString ();
                    if (version != pluginVersion) {
                        dynamicLibrary.Unload ();
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Plugin version mismatch; have: %s, expecting: %s.",
                            pluginVersion.c_str (),
                            version.c_str ());
                    }
                    interface.Initialize ();
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "%s is missing the plugin interface.",
                        path.c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s failed integrity check.",
                    path.c_str ());
            }
        }

        void Plugins::Plugin::Unload () {
            GetPluginInterfaceProc GetPluginInterface =
                (GetPluginInterfaceProc)dynamicLibrary.GetProc ("GetPluginInterface");
            if (GetPluginInterface != 0) {
                Interface &interface = GetPluginInterface ();
                interface.Shutdown ();
                dynamicLibrary.Unload ();
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s is missing the plugin interface.",
                    path.c_str ());
            }
        }

        namespace {
            const char * const TAG_PLUGINS = "plugins";
            const char * const ATTR_SCHEMA_VERSION = "schema_version";

            const char * const TAG_PLUGIN = "plugin";
            const char * const ATTR_PATH = "path";
            const char * const ATTR_VERSION = "version";
            const char * const ATTR_SHA2_256 = "SHA2-256";

            const char * const TAG_DEPENDENCIES = "dependencies";
            const char * const TAG_DEPENDENCY = "dependency";

            const ui32 PLUGINS_XML_SCHEMA_VERSION = 1;
        }

        Plugins::Plugins (
                const std::string &path_,
                std::size_t maxPluginsFileSize) :
                path (path_),
                modified (false) {
            if (Path (path).Exists ()) {
                ReadOnlyFile file (HostEndian, path);
                // Protect yourself.
                std::size_t fileSize = (std::size_t)file.GetSize ();
                if (fileSize > maxPluginsFileSize) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "'%s' is bigger (" THEKOGANS_UTIL_SIZE_T_FORMAT ") than expected. (" THEKOGANS_UTIL_SIZE_T_FORMAT ")",
                        path.c_str (),
                        fileSize,
                        maxPluginsFileSize);
                }
                Buffer buffer (HostEndian, fileSize);
                if (buffer.AdvanceWriteOffset (
                        file.Read (buffer.GetWritePtr (), fileSize)) != fileSize) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to read " THEKOGANS_UTIL_SIZE_T_FORMAT " bytes from '%s'.",
                        fileSize,
                        path.c_str ());
                }
                pugi::xml_document document;
                pugi::xml_parse_result result =
                    document.load_buffer (
                        buffer.GetReadPtr (),
                        buffer.GetDataAvailableForReading ());
                if (!result) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to parse %s (%s)",
                        path.c_str (),
                        result.description ());
                }
                pugi::xml_node node = document.document_element ();
                if (std::string (node.name ()) == TAG_PLUGINS) {
                    ParsePlugins (node);
                }
            }
        }

        Plugins::Plugin::Ptr Plugins::GetPlugin (const std::string &path) const {
            PluginMap::const_iterator it = plugins.find (path);
            return it != plugins.end () ? it->second : Plugin::Ptr ();
        }

        void Plugins::AddPlugin (
                const std::string &path,
                const std::string &version,
                const std::string &SHA2_256,
                const Plugin::Dependencies &dependencies) {
            Plugin::Ptr plugin = GetPlugin (path);
            if (plugin.Get () == 0) {
                plugin.Reset (new Plugin (path, version, SHA2_256, dependencies));
                plugins.insert (PluginMap::value_type (path, plugin));
                modified = true;
            }
            else if (plugin->version != version ||
                    plugin->SHA2_256 != SHA2_256 ||
                    plugin->dependencies != dependencies) {
                plugin->version = version;
                plugin->SHA2_256 = SHA2_256;
                plugin->dependencies = dependencies;
                modified = true;
            }
        }

        void Plugins::DeletePlugin (const std::string &path) {
            PluginMap::iterator it = plugins.find (path);
            if (it != plugins.end ()) {
                plugins.erase (it);
                modified = true;
            }
        }

        void Plugins::DeletePlugins () {
            if (!plugins.empty ()) {
                plugins.clear ();
                modified = true;
            }
        }

        void Plugins::Save () {
            if (modified) {
                std::fstream pluginsFile (
                    path.c_str (),
                    std::fstream::out | std::fstream::trunc);
                if (pluginsFile.is_open ()) {
                    Attributes attributes;
                    attributes.push_back (Attribute (ATTR_SCHEMA_VERSION, ui32Tostring (PLUGINS_XML_SCHEMA_VERSION)));
                    pluginsFile << OpenTag (0, TAG_PLUGINS, attributes, false, true);
                    for (PluginMap::const_iterator
                            it = plugins.begin (),
                            end = plugins.end (); it != end; ++it) {
                        Attributes attributes;
                        attributes.push_back (Attribute (ATTR_PATH, EncodeXMLCharEntities (it->second->path)));
                        attributes.push_back (Attribute (ATTR_VERSION, it->second->version));
                        attributes.push_back (Attribute (ATTR_SHA2_256, it->second->SHA2_256));
                        if (it->second->dependencies.empty ()) {
                            pluginsFile << OpenTag (1, TAG_PLUGIN, attributes, true, true);
                        }
                        else {
                            pluginsFile <<
                                OpenTag (1, TAG_PLUGIN, attributes, false, true) <<
                                OpenTag (2, TAG_DEPENDENCIES, Attributes (), false, true);
                            for (Plugin::Dependencies::const_iterator
                                    jt = it->second->dependencies.begin (),
                                    end = it->second->dependencies.end (); jt != end; ++jt) {
                                pluginsFile << OpenTag (3, TAG_DEPENDENCY) <<
                                    EncodeXMLCharEntities (*jt) << CloseTag (0, TAG_DEPENDENCY);
                            }
                            pluginsFile <<
                                CloseTag (2, TAG_DEPENDENCIES) <<
                                CloseTag (1, TAG_PLUGIN);
                        }
                    }
                    pluginsFile << CloseTag (0, TAG_PLUGINS);
                    modified = false;
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unable to open: %s.",
                        path.c_str ());
                }
            }
        }

        void Plugins::Load () {
            std::string directory = Path (path).GetDirectory ();
            for (PluginMap::iterator
                    it = plugins.begin (),
                    end = plugins.end (); it != end; ++it) {
                it->second->Load (directory);
            }
        }

        void Plugins::Unload () {
            for (PluginMap::iterator
                    it = plugins.begin (),
                    end = plugins.end (); it != end; ++it) {
                it->second->Unload ();
            }
        }

        void Plugins::ParsePlugins (pugi::xml_node &node) {
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element) {
                    std::string childName = child.name ();
                    if (childName == TAG_PLUGIN) {
                        ParsePlugin (child);
                    }
                }
            }
        }

        void Plugins::ParsePlugin (pugi::xml_node &node) {
            std::string path = Decodestring (node.attribute (ATTR_PATH).value ());
            if (!path.empty ()) {
                std::string version = node.attribute (ATTR_VERSION).value ();
                if (!version.empty ()) {
                    std::string SHA2_256 = node.attribute (ATTR_SHA2_256).value ();
                    if (!SHA2_256.empty ()) {
                        Plugin::Ptr plugin (new Plugin (path, version, SHA2_256));
                        for (pugi::xml_node child = node.first_child ();
                                !child.empty (); child = child.next_sibling ()) {
                            if (child.type () == pugi::node_element) {
                                std::string childName = child.name ();
                                if (childName == TAG_DEPENDENCIES) {
                                    ParseDependencies (child, *plugin);
                                }
                            }
                        }
                        plugins.insert (PluginMap::value_type (path, plugin));
                    }
                    else {
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                            "Empty SHA2_256 attribute for plugin %s.",
                            path.c_str ());
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Empty version attribute for plugin %s.",
                        path.c_str ());
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "Empty path attribute in plugin.");
            }
        }

        void Plugins::ParseDependencies (
                pugi::xml_node &node,
                Plugin &plugin) {
            for (pugi::xml_node child = node.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                if (child.type () == pugi::node_element) {
                    std::string childName = child.name ();
                    if (childName == TAG_DEPENDENCY) {
                        std::string dependency = TrimSpaces (child.text ().get ());
                        if (!dependency.empty ()) {
                            plugin.dependencies.insert (dependency);
                        }
                    }
                }
            }
        }

    } // namespace util
} // namespace thekogans
