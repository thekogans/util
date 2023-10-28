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

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows)

#include "thekogans/util/Types.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/os/windows/WindowsFirewall.h"

namespace thekogans {
    namespace util {
        namespace os {
            namespace windows {

                bool WindowsFirewall::IsOn () {
                    VARIANT_BOOL enabled = VARIANT_FALSE;
                    HRESULT hr = profile.profile->get_FirewallEnabled (&enabled);
                    if (SUCCEEDED (hr)) {
                        return  enabled == VARIANT_TRUE;
                    }
                    else {
                        THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                    }
                }

                void WindowsFirewall::TurnOn () {
                    if (!IsOn ()) {
                        HRESULT hr = profile.profile->put_FirewallEnabled (VARIANT_TRUE);
                        if (FAILED (hr)) {
                            THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                        }
                    }
                }

                void WindowsFirewall::TurnOff () {
                    if (IsOn ()) {
                        HRESULT hr = profile.profile->put_FirewallEnabled (VARIANT_FALSE);
                        if (FAILED (hr)) {
                            THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                        }
                    }
                }

                namespace {
                    struct BStr {
                        BSTR bstr;
                        explicit BStr (const std::string &str) :
                                bstr (SysAllocString (UTF8ToUTF16 (str).c_str ())) {
                            if (bstr != 0) {
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                                    "Unable to covert '%s' to BSTR.",
                                    str.c_str ());
                            }
                        }
                        ~BStr () {
                            SysFreeString (bstr);
                        }
                    };

                    struct Apps {
                        INetFwAuthorizedApplications *apps;
                        Apps (INetFwProfile *profile) :
                                apps (0) {
                            HRESULT hr = profile->get_AuthorizedApplications (&apps);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                        ~Apps () {
                            apps->Release ();
                        }

                        INetFwAuthorizedApplication *GetApp (const std::string &path) {
                            INetFwAuthorizedApplication *app = 0;
                            HRESULT hr = apps->Item (BStr (path).bstr, &app);
                            if (SUCCEEDED (hr) || HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
                                return app;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }

                        }

                        void AddApp (INetFwAuthorizedApplication *app) {
                            HRESULT hr = apps->Add (app);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void RemoveApp (const std::string &path) {
                            HRESULT hr = apps->Remove (BStr (path).bstr);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                    };

                    struct App {
                        INetFwAuthorizedApplication *app;
                        App (INetFwAuthorizedApplication *app_ = 0) :
                                app (app_) {
                            if (app == 0) {
                                HRESULT hr = CoCreateInstance (
                                    __uuidof (NetFwAuthorizedApplication),
                                    0,
                                    CLSCTX_INPROC_SERVER,
                                    __uuidof (INetFwAuthorizedApplication),
                                    (void **)&app);
                                if (FAILED (hr)) {
                                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                                }
                            }
                        }
                        ~App () {
                            app->Release ();
                        }

                        bool IsEnabled () {
                            VARIANT_BOOL enabled;
                            HRESULT hr = app->get_Enabled (&enabled);
                            if (SUCCEEDED (hr)) {
                                return enabled == VARIANT_TRUE;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void SetPath (const std::string &path) {
                            HRESULT hr = app->put_ProcessImageFileName (BStr (path).bstr);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void SetName (const std::string &name) {
                            HRESULT hr = app->put_Name (BStr (name).bstr);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                    };
                }

                bool WindowsFirewall::IsAppEnabled (const std::string &path) {
                    Apps apps (profile.profile);
                    App app (apps.GetApp (path));
                    return app.IsEnabled ();
                }

                void WindowsFirewall::EnableApp (
                        const std::string &path,
                        const std::string &name) {
                    if (!IsAppEnabled (path)) {
                        Apps apps (profile.profile);
                        App app;
                        app.SetPath (path);
                        app.SetName (name);
                        apps.AddApp (app.app);
                    }
                }

                void WindowsFirewall::DisableApp (const std::string &path) {
                    if (IsAppEnabled (path)) {
                        Apps apps (profile.profile);
                        apps.RemoveApp (path);
                    }
                }

                namespace {
                    struct Ports {
                        INetFwOpenPorts *ports;
                        Ports (INetFwProfile *profile) :
                                ports (0) {
                            HRESULT hr = profile->get_GloballyOpenPorts (&ports);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                        ~Ports () {
                            ports->Release ();
                        }

                        INetFwOpenPort *GetPort (
                                ui16 portNumber,
                                NET_FW_IP_PROTOCOL protocol) {
                            INetFwOpenPort *port = 0;
                            HRESULT hr = ports->Item (portNumber, protocol, &port);
                            if (SUCCEEDED (hr) || HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
                                return port;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }

                        }

                        void AddPort (INetFwOpenPort *port) {
                            HRESULT hr = ports->Add (port);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void RemovePort (
                                ui16 portNumber,
                                NET_FW_IP_PROTOCOL protocol) {
                            HRESULT hr = ports->Remove (portNumber, protocol);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                    };

                    struct Port {
                        INetFwOpenPort *port;
                        Port (INetFwOpenPort *port_ = 0) :
                                port (port_) {
                            if (port == 0) {
                                HRESULT hr = CoCreateInstance (
                                    __uuidof (NetFwOpenPort),
                                    0,
                                    CLSCTX_INPROC_SERVER,
                                    __uuidof (INetFwOpenPort),
                                    (void **)&port);
                                if (FAILED (hr)) {
                                    THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                                }
                            }
                        }
                        ~Port () {
                            port->Release ();
                        }

                        bool IsEnabled () {
                            VARIANT_BOOL enabled;
                            HRESULT hr = port->get_Enabled (&enabled);
                            if (SUCCEEDED (hr)) {
                                return enabled == VARIANT_TRUE;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void SetPortNumber (ui16 portNumber) {
                            HRESULT hr = port->put_Port (portNumber);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void SetProtocol (NET_FW_IP_PROTOCOL protocol) {
                            HRESULT hr = port->put_Protocol (protocol);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }

                        void SetName (const std::string &name) {
                            HRESULT hr = port->put_Name (BStr (name).bstr);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                    };
                }

                bool WindowsFirewall::IsPortEnabled (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol) {
                    Ports ports (profile.profile);
                    Port port (ports.GetPort (portNumber, protocol));
                    return port.IsEnabled ();
                }

                void WindowsFirewall::EnablePort (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol,
                        const std::string &name) {
                    if (!IsPortEnabled (portNumber, protocol)) {
                        Ports ports (profile.profile);
                        Port port;
                        port.SetPortNumber (portNumber);
                        port.SetProtocol (protocol);
                        port.SetName (name);
                        ports.AddPort (port.port);
                    }
                }

                void WindowsFirewall::DisablePort (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol) {
                    if (IsPortEnabled (portNumber, protocol)) {
                        Ports ports (profile.profile);
                        ports.RemovePort (portNumber, protocol);
                    }
                }

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)
