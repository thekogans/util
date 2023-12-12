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

#if !defined (__thekogans_util_os_windows_WindowsFirewall_h)
#define __thekogans_util_os_windows_WindowsFirewall_h

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_Windows)

#include "thekogans/util/os/windows/WindowsHeader.h"
#include <netfw.h>
#include <objbase.h>
#include <oleauto.h>

namespace thekogans {
    namespace util {
        namespace os {
            namespace windows {

                /// \struct WindowsFirewall WindowsFirewall.h thekogans/util/os/windows/WindowsFirewall.h
                ///
                /// \brief
                /// WindowsFirewall puts a thin wrapper around the Windows firewall COM interface.
                /// It exposes methods for querying and setting the firewall status as well as
                /// enabling/disabling applications and ports.

                struct _LIB_THEKOGANS_UTIL_DECL WindowsFirewall {
                private:
                    /// \struct WindowsFirewall::Mgr WindowsFirewall.h thekogans/util/WindowsFirewall.h
                    ///
                    /// \brief
                    /// Wrap INetFwMgr COM object to provide lifetime management.
                    struct Mgr {
                        /// \brief
                        /// Windows COM object.
                        INetFwMgr *mgr;
                        /// \brief
                        /// ctor.
                        Mgr () :
                                mgr (0) {
                            HRESULT hr = CoCreateInstance (
                                __uuidof (NetFwMgr),
                                0,
                                CLSCTX_INPROC_SERVER,
                                __uuidof (INetFwMgr),
                                (void **)&mgr);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                        /// \brief
                        /// dtor.
                        ~Mgr () {
                            mgr->Release ();
                        }
                    } mgr;
                    /// \struct WindowsFirewall::Policy WindowsFirewall.h thekogans/util/WindowsFirewall.h
                    ///
                    /// \brief
                    /// Wrap INetFwPolicy COM object to provide lifetime management.
                    struct Policy {
                        /// \brief
                        /// Windows COM object.
                        INetFwPolicy *policy;
                        /// \brief
                        /// ctor.
                        explicit Policy (INetFwMgr *mgr) :
                                policy (0) {
                            HRESULT hr = mgr->get_LocalPolicy (&policy);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                        /// \brief
                        /// dtor.
                        ~Policy () {
                            policy->Release ();
                        }
                    } policy;
                    /// \struct WindowsFirewall::Profile WindowsFirewall.h thekogans/util/WindowsFirewall.h
                    ///
                    /// \brief
                    /// Wrap INetFwProfile COM object to provide lifetime management.
                    struct Profile {
                        /// \brief
                        /// Windows COM object.
                        INetFwProfile *profile;
                        /// \brief
                        /// ctor.
                        explicit Profile (INetFwPolicy *policy) :
                                profile (0) {
                            HRESULT hr = policy->get_CurrentProfile (&profile);
                            if (FAILED (hr)) {
                                THEKOGANS_UTIL_THROW_HRESULT_ERROR_CODE_EXCEPTION (hr);
                            }
                        }
                        /// \brief
                        /// dtor.
                        ~Profile () {
                            profile->Release ();
                        }
                    } profile;

                public:
                    /// \brief
                    /// ctor.
                    WindowsFirewall () :
                        policy (mgr.mgr),
                        profile (policy.policy) {}

                    /// \brief
                    /// Return true if firewall is enabled.
                    /// \return true==firewall is enabled.
                    bool IsOn ();
                    /// \brief
                    /// Eable the firewall if not already enabled.
                    void TurnOn();
                    /// \brief
                    /// Disable the firewall if not already disabled.
                    void TurnOff ();

                    /// \brief
                    /// Return true if the given application path is enavbled.
                    /// \return true==the given application path is enavbled.
                    bool IsAppEnabled (const std::string &path);
                    /// \brief
                    /// Given an application path, enable if not already.
                    /// \param[in] path Application path.
                    /// \param[in] name Name to give the firewall entry.
                    void EnableApp (
                        const std::string &path,
                        const std::string &name);
                    /// \brief
                    /// Given an application path, disable if not already.
                    /// \param[in] path Application path.
                    void DisableApp (const std::string &path);

                    /// \brief
                    /// Return true if the given port is enavbled for the given protocol.
                    /// \return true==the given port is enavbled for the given protocol.
                    bool IsPortEnabled (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol);
                    /// \brief
                    /// Given an application port and protocol, enable if not already.
                    /// \param[in] port Port to enable.
                    /// \param[in] protocol Protocol to enable.
                    /// \param[in] name Name to give the firewall entry.
                    void EnablePort (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol,
                        const std::string &name);
                    /// \brief
                    /// Given a port and protocol, disable if not already.
                    /// \param[in] path Port to disable.
                    /// \param[in] protocol Protocol to disable.
                    void DisablePort (
                        ui16 portNumber,
                        NET_FW_IP_PROTOCOL protocol);
                };

            } // namespace windows
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)

#endif // !defined (__thekogans_util_os_windows_WindowsFirewall_h)
