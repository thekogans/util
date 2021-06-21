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

#if !defined (__thekogans_util_WindowsUtils_h)
#define __thekogans_util_WindowsUtils_h

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
#include <memory>
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/Rectangle.h"

/// \brief
/// Create both ends of an anonymous pipe. Useful
/// if you're planning on using it for overlapped io.
/// Adapted from: http://www.davehart.net/remote/PipeEx.c
/// \param[out] fildes fildes[0] = read end of the pipe,
/// fildes[1] = write end of the pipe.
/// \return 0 = success, -1 = errno contains the error.
_LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API pipe (
    THEKOGANS_UTIL_HANDLE fildes[2]);

namespace thekogans {
    namespace util {

        /// \brief
        /// Convert a given i64 value to Windows FILETIME.
        /// NOTE: value = 0 -> midnight 1/1/1970.
        /// \param[in] value i64 value to convert.
        /// \return Converted FILETIME.
        _LIB_THEKOGANS_UTIL_DECL FILETIME _LIB_THEKOGANS_UTIL_API i64ToFILETIME (i64 value);
        /// \brief
        /// Convert a given Windows FILETIME value to i64.
        /// \param[in] value FILETIME value to convert.
        /// \return Converted i64.
        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API FILETIMEToi64 (const FILETIME &value);

        /// \brief
        /// Convert the given multibyte string to UTF16.
        /// NOTE: The following is allowed and will result in std::wstring ():
        /// multiByte == 0 && length == 0
        /// multiByte != 0 && length == 0
        /// The following will result in THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL being thrown:
        /// multiByte == 0 && length > 0
        /// \param[in] codePage Code page to use in performing the conversion.
        /// \param[in] multiByte Multibyte string to convert.
        /// \param[in] length Length (in bytes) of the given UTF8 string.
        /// \param[in] flags Various MB_* flags to control the behavior of MultiByteToWideChar.
        /// \return std::wstring UTF16 representation of the given UTF8 string.
        _LIB_THEKOGANS_UTIL_DECL std::wstring _LIB_THEKOGANS_UTIL_API MultiByteToUTF16 (
            UINT codePage,
            const char *multiByte,
            std::size_t length,
            DWORD flags = MB_ERR_INVALID_CHARS);

        /// \brief
        /// Convert the given UTF8 string to UTF16.
        /// NOTE: The following is allowed and will result in std::wstring ():
        /// utf8 == 0 && length == 0
        /// utf8 != 0 && length == 0
        /// The following will result in THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL being thrown:
        /// utf8 == 0 && length > 0
        /// \param[in] utf8 UTF8 string to convert.
        /// \param[in] length Length (in bytes) of the given UTF8 string.
        /// \param[in] flags Various MB_* flags to control the behavior of MultiByteToWideChar.
        /// \return std::wstring UTF16 representation of the given UTF8 string.
        inline std::wstring _LIB_THEKOGANS_UTIL_API UTF8ToUTF16 (
                const char *utf8,
                std::size_t length,
                DWORD flags = MB_ERR_INVALID_CHARS) {
            return MultiByteToUTF16 (CP_UTF8, utf8, length, flags);
        }

        /// \brief
        /// Convert the given UTF8 string to UTF16.
        /// \param[in] utf8 UTF8 string to convert.
        /// \param[in] flags Various MB_* flags to control the behavior of MultiByteToWideChar.
        /// \return std::wstring UTF16 representation of the given UTF8 string.
        inline std::wstring _LIB_THEKOGANS_UTIL_API UTF8ToUTF16 (
                const std::string &utf8,
                DWORD flags = MB_ERR_INVALID_CHARS) {
            return UTF8ToUTF16 (utf8.data (), utf8.size (), flags);
        }

        /// \brief
        /// Convert the given ACP string to UTF16.
        /// \param[in] acp ACP string to convert.
        /// \param[in] length Length (in bytes) of the given ACP string.
        /// \param[in] flags Various MB_* flags to control the behavior of MultiByteToWideChar.
        /// \return std::wstring UTF16 representation of the given ACP string.
        inline std::wstring _LIB_THEKOGANS_UTIL_API ACPToUTF16 (
                const char *acp,
                std::size_t length,
                DWORD flags = MB_ERR_INVALID_CHARS) {
            return MultiByteToUTF16 (CP_ACP, acp, length, flags);
        }
        /// \brief
        /// Convert the given ACP string to UTF16.
        /// \param[in] acp ACP string to convert.
        /// \param[in] flags Various MB_* flags to control the behavior of MultiByteToWideChar.
        /// \return std::wstring UTF16 representation of the given ACP string.
        inline std::wstring _LIB_THEKOGANS_UTIL_API ACPToUTF16(
                const std::string &acp,
                DWORD flags = MB_ERR_INVALID_CHARS) {
            return ACPToUTF16 (acp.data (), acp.size (), flags);
        }

        #if !defined (WC_ERR_INVALID_CHARS)
            #define WC_ERR_INVALID_CHARS 0x00000080
        #endif // !defined (WC_ERR_INVALID_CHARS)

        /// \brief
        /// Convert the given UTF16 string to UTF8.
        /// NOTE: The following is allowed and will result in std::string ():
        /// utf16 == 0 && length == 0
        /// utf16 != 0 && length == 0
        /// The following will result in THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL being thrown:
        /// utf16 == 0 && length > 0
        /// \param[in] utf16 UTF16 string to convert.
        /// \param[in] length Length (in sizeof (wchar_t)) of the given UTF16 string.
        /// \param[in] flags Various WC_* flags to control the behavior of WideCharToMultiByte.
        /// \return std::string UTF8 representation of the given UTF16 string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API UTF16ToUTF8 (
            const wchar_t *utf16,
            std::size_t length,
            DWORD flags = WC_ERR_INVALID_CHARS);
        /// \brief
        /// Convert the given UTF16 string to UTF8.
        /// \param[in] utf16 UTF16 string to convert.
        /// \param[in] flags Various WC_* flags to control the behavior of WideCharToMultiByte.
        /// \return std::string UTF8 representation of the given UTF16 string.
        inline std::string _LIB_THEKOGANS_UTIL_API UTF16ToUTF8 (
                const std::wstring &utf16,
                DWORD flags = WC_ERR_INVALID_CHARS) {
            return UTF16ToUTF8 (utf16.data (), utf16.size (), flags);
        }

        /// \struct HGLOBALPtr WindowsUtils.h thekogans/util/WindowsUtils.h
        ///
        /// \brief
        /// A helper used to make dealing with Windows HGLOBAL api easier.

        struct _LIB_THEKOGANS_UTIL_DECL HGLOBALPtr {
        private:
            /// \brief
            /// Contained HGLOBAL.
            HGLOBAL hglobal;
            /// \brief
            /// true == call GlobalFree when done.
            bool owner;
            /// \brief
            /// Result of GlobalLock.
            void *ptr;
            /// \brief
            /// Length of HGLOBAL block.
            std::size_t length;

        public:
            /// \brief
            /// Move ctor.
            /// \param[in,out] other HGLOBALPtr to move.
            HGLOBALPtr (HGLOBALPtr &&other) :
                    hglobal (0),
                    owner (false),
                    ptr (0),
                    length (0) {
                swap (other);
            }
            /// \brief
            /// ctor.
            /// \param[in] flags GlobalAlloc flags.
            /// \param[in] length_ GlobalAlloc dwBytes.
            HGLOBALPtr (
                UINT flags,
                SIZE_T length_) :
                hglobal (GlobalAlloc (flags, length_)),
                owner (hglobal != 0),
                ptr (owner ? GlobalLock (hglobal) : 0),
                length (owner ? length_ : 0) {}
            /// \brief
            /// ctor.
            /// \param[in] hglobal_ HGLOBAL to attach to.
            /// \param[in] owner_ true == call GlobalFree when done.
            HGLOBALPtr (
                    HGLOBAL hglobal_ = 0,
                    bool owner_ = true) :
                    hglobal (0),
                    owner (false),
                    ptr (0),
                    length (0) {
                Attach (hglobal_, owner_);
            }
            /// \brief
            /// dtor.
            ~HGLOBALPtr () {
                Attach (0, false);
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other Buffer to move.
            /// \return *this.
            HGLOBALPtr &operator = (HGLOBALPtr &&other);

            /// \brief
            /// std::swap for HGLOBALPtr.
            /// \param[in,out] other HGLOBALPtr to swap.
            void swap (HGLOBALPtr &other);

            /// \brief
            /// Type cast operator template.
            template<typename T>
            inline operator T * () const {
                return (T *)ptr;
            }

            /// \brief
            /// Return the length of HGLOBAL block.
            inline std::size_t GetLength () const {
                return length;
            }

            /// \brief
            /// Return the contained HGLOBAL.
            /// \return Contained HGLOBAL.
            inline HGLOBAL Get () const {
                return hglobal;
            }
            /// \brief
            /// Reset the contained HGLOBAL and attach to the given one.
            /// \param[in] hglobal_ HGLOBAL to attach to.
            /// \param[in] owner true == call GlobalFree when done.
            void Attach (
                HGLOBAL hglobal_,
                bool owner_);
            /// \brief
            /// Release and return the contained HGLOBAL.
            /// \return Contained HGLOBAL.
            HGLOBAL Release ();

            /// \brief
            /// HGLOBALPtr is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (HGLOBALPtr)
        };

        /// \struct WindowClass WindowsUtils.h thekogans/util/WindowsUtils.h
        ///
        /// \brief
        /// A helper for creating window classes.

        struct _LIB_THEKOGANS_UTIL_DECL WindowClass {
            /// \brief
            /// Class name.
            std::string name;
            /// \brief
            /// Module instance handle.
            HINSTANCE instance;
            /// \brief
            /// Registerd class atom.
            ATOM atom;

            /// \brief
            /// ctor.
            /// \param[in] name_ Class name.
            /// \param[in] wndProc Window message handler.
            /// \param[in] style Window class style.
            /// \param[in] icon = Window class icon.
            /// \param[in] cursor = Window class cursor.
            /// \param[in] background Window class background brush.
            /// \param[in] menu Window menu resource name.
            /// \param[in] instance_ Module instance handle.
            WindowClass (
                const std::string &name_,
                WNDPROC wndProc,
                UINT style = CS_HREDRAW | CS_VREDRAW,
                HICON icon = 0,
                HCURSOR cursor = LoadCursor (0, IDC_ARROW),
                HBRUSH background = (HBRUSH)(COLOR_WINDOW + 1),
                LPCTSTR menu = 0,
                HINSTANCE instance_ = GetModuleHandle (0));
            /// \brief
            /// dtor.
            virtual ~WindowClass ();
        };

        /// \struct Window WindowsUtils.h thekogans/util/WindowsUtils.h
        ///
        /// \brief
        /// A helper for creating windows. Hides a lot of Windows specific code and
        /// defaults almost everything. Used by \see{SystemRunLoop}.

        struct _LIB_THEKOGANS_UTIL_DECL Window {
            /// \brief
            /// Convenient typedef for std::unique_ptr<Window>.
            typedef std::unique_ptr<Window> UniquePtr;

            /// \brief
            /// Windows window.
            HWND wnd;
            /// \brief
            /// true == call DestroyWindow in the dtor.
            bool owner;

            /// \brief
            /// ctor.
            /// \param[in] wnd_ Window handle to wrap.
            /// \param[in] owner_ true == call DestroyWindow in the dtor.
            Window (
                HWND wnd_,
                bool owner_ = false) :
                wnd (wnd_),
                owner (owner_) {}
            /// \brief
            /// ctor.
            /// \param[in] windowClass An instance of \see{WindowClass}.
            /// \param[in] rectangle Window origin and extents.
            /// \param[in] name Window name.
            /// \param[in] style Window style.
            /// \param[in] extendedStyle Extended window style.
            /// \param[in] parent Window parent.
            /// \param[in] menu Window menu.
            /// \param[in] userInfo Passed to window proc in WM_CREATE message.
            Window (
                const WindowClass &windowClass,
                const Rectangle &rectangle = Rectangle (),
                const std::string &name = std::string (),
                DWORD style = WS_POPUP | WS_VISIBLE,
                DWORD extendedStyle = WS_EX_TOOLWINDOW,
                HWND parent = 0,
                HMENU menu = 0,
                void *userInfo = 0);
            /// \brief
            /// dtor.
            virtual ~Window ();
        };

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Windows)

#endif // !defined (__thekogans_util_WindowsUtils_h)
