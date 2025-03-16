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

#if !defined (__thekogans_util_StringUtils_h)
#define __thekogans_util_StringUtils_h

#include <cstddef>
#include <cstdarg>
#include <string>
#include <vector>
#include <list>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// std::size_t format string.
        #define THEKOGANS_UTIL_SIZE_T_FORMAT "%zu"
        /// \brief
        /// Format strings for various integral types.
    #if defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// i32 format string.
        #define THEKOGANS_UTIL_I32_FORMAT "%I32d"
        /// \brief
        /// ui32 format string.
        #define THEKOGANS_UTIL_UI32_FORMAT "%I32u"
        /// \brief
        /// i64 format string.
        #define THEKOGANS_UTIL_I64_FORMAT "%I64d"
        /// \brief
        /// ui64 format string.
        #define THEKOGANS_UTIL_UI64_FORMAT "%I64u"
    #else // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// i32 format string.
        #define THEKOGANS_UTIL_I32_FORMAT "%d"
        /// \brief
        /// ui32 format string.
        #define THEKOGANS_UTIL_UI32_FORMAT "%u"
        /// \brief
        /// i64 format string.
        #define THEKOGANS_UTIL_I64_FORMAT "%lld"
        /// \brief
        /// ui64 format string.
        #define THEKOGANS_UTIL_UI64_FORMAT "%llu"
    #endif // defined (TOOLCHAIN_OS_Windows)
        /// \brief
        /// f64 format string.
        #define THEKOGANS_UTIL_F32_FORMAT "%f"
        /// \brief
        /// f64 format string.
        #define THEKOGANS_UTIL_F64_FORMAT "%g"

        /// \brief
        /// Copy source to destination.
        /// NOTE: The destination will be \0 terminated even if source is truncated.
        /// \param[in] destination Where to place the copy.
        /// \param[in] destinationLength Number of characters in destination.
        /// \param[in] source String to copy.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API CopyString (
            char *destination,
            std::size_t destinationLength,
            const char *source);
        /// \brief
        /// Trim leading spaces.
        /// \param[in] str Pointer to string to trim spaces from.
        /// \return String with leading spaces trimmed.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimLeftSpaces (
            const char *str);
        /// \brief
        /// Trim trailing spaces.
        /// \param[in] str Pointer to string to trim spaces from.
        /// \return String with trailing spaces trimmed.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimRightSpaces (
            const char *str);
        /// \brief
        /// Trim leading and trailing spaces.
        /// \param[in] str Pointer to string to trim spaces from.
        /// \return String with leading and trailing spaces trimmed.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimSpaces (
            const char *str);
        /// \brief
        /// Convert a given string to upper case.
        /// \param[in] str Pointer to string to convert.
        /// \return String converted to upper case.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToUpper (
            const char *str);
        /// \brief
        /// Convert a given string to lower case.
        /// \param[in] str Pointer to string to convert.
        /// \return String converted to lower case.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToLower (
            const char *str);
        /// \brief
        /// Compare two strings ignoring case.
        /// \param[in] str1 First string to compare.
        /// \param[in] str2 Second string to compare.
        /// \return < 0 == str1 < str2, = 0 == str1 == str2, > 0 == str1 > str2.
        _LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API StringCompareIgnoreCase (
            const char *str1,
            const char *str2);
        /// \brief
        /// Compare two strings ignoring case.
        /// \param[in] str1 First string to compare.
        /// \param[in] str2 Second string to compare.
        /// \param[in] count Count of characters to compare.
        /// \return < 0 == str1 < str2, = 0 == str1 == str2, > 0 == str1 > str2.
        _LIB_THEKOGANS_UTIL_DECL int _LIB_THEKOGANS_UTIL_API StringCompareIgnoreCase (
            const char *str1,
            const char *str2,
            std::size_t count);
        /// \brief
        /// The IsUTF8String () function scans the '\0'-terminated string starting
        /// at utf8. It returns a pointer to the first byte of the first malformed
        /// or overlong UTF-8 sequence found, or NULL if the string contains
        /// only correct UTF-8. It also spots UTF-8 sequences that could cause
        /// trouble if converted to UTF-16, namely surrogate characters
        /// (U+D800..U+DFFF) and non-Unicode positions (U+FFFE..U+FFFF). This
        /// routine is very likely to find a malformed sequence if the input
        /// uses any other encoding than UTF-8. It therefore can be used as a
        /// very effective heuristic for distinguishing between UTF-8 and other
        /// encodings.
        ///
        /// I wrote this code mainly as a specification of functionality; there
        /// are no doubt performance optimizations possible for certain CPUs.
        ///
        /// Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/> -- 2005-03-30
        /// License: http://www.cl.cam.ac.uk/~mgk25/short-license.html
        /// \param[in] utf8 Pointer to UTF-8 encoded string to test.
        /// \param[in] length Length (in bytes) of UTF-8 encoded string to test.
        /// \param[out] utf8Length Optional. Will receive the UTF8 length if the given string is valid.
        /// \return If invalid, pointer to first malformed sequence, 0 if valid UTF-8.
        _LIB_THEKOGANS_UTIL_DECL const char * _LIB_THEKOGANS_UTIL_API IsUTF8String (
            const char *utf8,
            std::size_t length,
            std::size_t *utf8Length = nullptr);
        /// \brief
        /// Check if the given string is valid UTF8.
        /// \param[in] utf8 String to check.
        /// \return If invalid, pointer to first malformed sequence, 0 if valid UTF-8.
        inline const char *IsUTF8String (const std::string &utf8) {
            return IsUTF8String (utf8.data (), utf8.size ());
        }
        /// \brief
        /// Return the length (in UTF8 characters) of the given UTF8 string.
        /// \param[in] utf8 UTF8 string whose length to return.
        /// \param[in] length Length (in bytes) of the given string.
        /// \return The length (in UTF8 characters) of the given UTF8 string.
        inline std::size_t UTF8StringLength (
                const char *utf8,
                std::size_t length) {
            std::size_t utf8Length = 0;
            return IsUTF8String (utf8, length, &utf8Length) == nullptr ? utf8Length : 0;
        }
        /// \brief
        /// Return the length (in UTF8 characters) of the given UTF8 string.
        /// \param[in] utf8 UTF8 string whose length to return.
        /// \return The length (in UTF8 characters) of the given UTF8 string.
        inline std::size_t UTF8StringLength (const std::string &utf8) {
            return UTF8StringLength (utf8.data (), utf8.size ());
        }
        /// \brief
        /// Hex encode a given buffer.
        /// \param[in] buffer Pointer to buffer to be hex encoded.
        /// \param[in] length Length of buffer.
        /// \param[out] hexBuffer Where to write enecoded data.
        /// \return Number of bytes written to hexBuffer.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexEncodeBuffer (
            const void *buffer,
            std::size_t length,
            char *hexBuffer);
        /// \brief
        /// Hex encode a given buffer.
        /// \param[in] buffer Pointer to buffer to be hex encoded.
        /// \param[in] length Length of buffer.
        /// \return Hex encoded buffer.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodeBuffer (
            const void *buffer,
            std::size_t length);
        /// \brief
        /// Decode a previously hex encoded buffer.
        /// NOTE: The given hexBuffer must be even bytes long and contain only valid hex chars.
        /// \param[in] hexBuffer Pointer to buffer to be hex decoded.
        /// \param[in] length Length of buffer.
        /// \param[out] buffer Where to write decoded data.
        /// \return Number of bytes written to buffer.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexDecodeBuffer (
            const char *hexBuffer,
            std::size_t length,
            void *buffer);
        /// \brief
        /// Decode a previously hex encoded buffer.
        /// NOTE: The given hexBuffer must be even bytes long and contain only valid hex chars.
        /// \param[in] hexBuffer Pointer to buffer to be hex decoded.
        /// \param[in] length Length of buffer.
        /// \return Original buffer.
        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodeBuffer (
            const char *hexBuffer,
            std::size_t length);
        /// \brief
        /// Hex encode a given string.
        /// \param[in] str String to be hex encoded.
        /// \return Hex encoded string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodestring (
            const std::string &str);
        /// \brief
        /// Decode a previously hex encoded string.
        /// NOTE: The given hexString must be even bytes long and contain only valid hex chars.
        /// \param[in] hexString previously hex encoded string.
        /// \param[out] buffer Where to write decoded data.
        /// \return Number of bytes written to buffer.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HexDecodestring (
            const std::string &hexString,
            void *buffer);
        /// \brief
        /// Decode a previously hex encoded string.
        /// NOTE: The given hexString must be even bytes long and contain only valid hex chars.
        /// \param[in] hexString previously hex encoded string.
        /// \return Original string.
        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodestring (
            const std::string &hexString);
        /// \brief
        /// Hex format a buffer. This function will produce
        /// output very similar to hex editors. It's very
        /// convenient for dumping raw binary data to a log.
        /// \param[in] buffer Pointer to start of buffer.
        /// \param[in] length Buffer length.
        /// \return Hex formatted buffer suitable for
        /// writing to a log.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatBuffer (
            const void *buffer,
            std::size_t length);
        /// \brief
        /// A wrapper around HexFormatBuffer.
        /// Calls HexFormatBuffer (str[0], str.size ());
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatstring (
            const std::string &str);
        /// \brief
        /// Return a hash of the string mod hashTableSize.
        ///
        /// DJBX33A (Daniel J. Bernstein, Times 33 with Addition)
        ///
        /// This is Daniel J. Bernstein's popular `times 33' hash function as
        /// posted by him years ago on comp.lang.c. It basically uses a function
        /// like ``hash(i) = hash(i-1) * 33 + str[i]''. This is one of the best
        /// known hash functions for strings. Because it is both computed very
        /// fast and distributes very well.
        ///
        /// The magic of number 33, i.e. why it works better than many other
        /// constants, prime or not, has never been adequately explained by
        /// anyone. So I try an explanation: if one experimentally tests all
        /// multipliers between 1 and 256 (as RSE did now) one detects that even
        /// numbers are not useable at all. The remaining 128 odd numbers
        /// (except for the number 1) work more or less all equally well. They
        /// all distribute in an acceptable way and this way fill a hash table
        /// with an average percent of approx. 86%.
        ///
        /// If one compares the Chi^2 values of the variants, the number 33 not
        /// even has the best value. But the number 33 and a few other equally
        /// good numbers like 17, 31, 63, 127 and 129 have nevertheless a great
        /// advantage to the remaining numbers in the large set of possible
        /// multipliers: their multiply operation can be replaced by a faster
        /// operation based on just one shift plus either a single addition
        /// or subtraction operation. And because a hash function has to both
        /// distribute good _and_ has to be very fast to compute, those few
        /// numbers should be preferred and seems to be the reason why Daniel J.
        /// Bernstein also preferred it.
        ///
        ///
        /// -- Ralf S. Engelschall <rse@engelschall.com>
        ///
        /// \param[in] str String to hash.
        /// \param[in] length String length.
        /// \param[in] hashTableSize The final value will be 0 < value < hashTableSize.
        /// \return String hash.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HashString (
            const char *str,
            std::size_t length = SIZE_T_MAX,
            std::size_t hashTableSize = SIZE_T_MAX);
        /// \brief
        /// Return a hash of the string mod hashTableSize.
        /// \param[in] str String to hash.
        /// \param[in] hashTableSize The final value will be 0 < value < hashTableSize.
        /// \return String hash.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HashString (
            const std::string &str,
            std::size_t hashTableSize = SIZE_T_MAX);

        /// \brief
        /// Given a list of strings, return the longest common prefix.
        /// \param[in] strings List of strings whose prefix to return.
        /// \return The longest common prefix.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetLongestCommonPrefix (
            const std::list<std::string> &strings);

        /// \brief
        /// Format a list of strings with the given separator.
        /// \param[in] strings List of strings to format.
        /// \param[in] separator List entry separator.
        /// \return Formatted list.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatList (
            const std::list<std::string> &strings,
            const std::string &separator);

        /// \brief
        /// Parse a std::size_t represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return std::size_t represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API stringTosize_t (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Convert a string value to a boolean.
        /// \param[in] value Value to convert.
        /// \return true if value == "true", false if value == "false".
        _LIB_THEKOGANS_UTIL_DECL bool _LIB_THEKOGANS_UTIL_API stringTobool (
            const char *value,
            char **end = nullptr);
        /// \brief
        /// Parse a i8 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return i8 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL i8 _LIB_THEKOGANS_UTIL_API stringToi8 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a ui8 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return ui8 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL ui8 _LIB_THEKOGANS_UTIL_API stringToui8 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a i16 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return i16 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL i16 _LIB_THEKOGANS_UTIL_API stringToi16 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a ui16 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return ui16 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL ui16 _LIB_THEKOGANS_UTIL_API stringToui16 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a i32 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return i32 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL i32 _LIB_THEKOGANS_UTIL_API stringToi32 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a ui32 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return ui32 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API stringToui32 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a i64 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return i64 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API stringToi64 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a ui64 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \param[in] base Radix base of the number represented by value.
        /// \return ui64 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL ui64 _LIB_THEKOGANS_UTIL_API stringToui64 (
            const char *value,
            char **end = nullptr,
            i32 base = 10);
        /// \brief
        /// Parse a f32 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \return f32 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL f32 _LIB_THEKOGANS_UTIL_API stringTof32 (
            const char *value,
            char **end = nullptr);
        /// \brief
        /// Parse a f64 represented by a given string.
        /// \param[in] value Pointer to the beginning of the string.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \return f64 represented by a given string.
        _LIB_THEKOGANS_UTIL_DECL f64 _LIB_THEKOGANS_UTIL_API stringTof64 (
            const char *value,
            char **end = nullptr);
        /// \brief
        /// Parse an OS specific error code. On windows error codes
        /// are unsigned, and on POSIX they are signed. This api puts
        /// a uniform face on parsing system specific error codes.
        /// \param[in] value Pointer to the beginning of the error code.
        /// \param[out] end On return will hold a pointer where parsing
        /// has stopped (due to error?). It will be set to NULL if the
        /// entire value was consumed.
        /// \return An OS specific error code.
        _LIB_THEKOGANS_UTIL_DECL THEKOGANS_UTIL_ERROR_CODE _LIB_THEKOGANS_UTIL_API
            stringToTHEKOGANS_UTIL_ERROR_CODE (
                const char *value,
                char **end = nullptr);

        /// \brief
        /// Format a const void *.
        /// \param[in] value const void * to format.
        /// \return A string representing a formatted const void *.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API PointerTostring (
            const void *value);
        /// \brief
        /// Format a std::size_t.
        /// \param[in] value std::size_t to format.
        /// \param[in] format Format string.
        /// \return A string representing a formatted std::size_t.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API size_tTostring (
            std::size_t value,
            const char *format = THEKOGANS_UTIL_SIZE_T_FORMAT);
        /// \brief
        /// Convert a boolean value to a string.
        /// \param[in] value Value to convert.
        /// \return "true" if value == true, "false" if value == false.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API boolTostring (
            bool value);
        /// \brief
        /// Format an i32.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i32Tostring (
            i32 value,
            const char *format = THEKOGANS_UTIL_I32_FORMAT);
        /// \brief
        /// Format an ui32.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui32Tostring (
            ui32 value,
            const char *format = THEKOGANS_UTIL_UI32_FORMAT);
        /// \brief
        /// Format an i64.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i64Tostring (
            i64 value,
            const char *format = THEKOGANS_UTIL_I64_FORMAT);
        /// \brief
        /// Format an ui64.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui64Tostring (
            ui64 value,
            const char *format = THEKOGANS_UTIL_UI64_FORMAT);
        /// \brief
        /// Format an f32.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f32Tostring (
            f32 value,
            const char *format = THEKOGANS_UTIL_F32_FORMAT);
        /// \brief
        /// Format an f64.
        /// \param[in] value Value to format.
        /// \param[in] format printf style format to use.
        /// \return Formatted value.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f64Tostring (
            f64 value,
            const char *format = THEKOGANS_UTIL_F64_FORMAT);

        /// \brief
        /// Format a string from a list of arguments.
        /// \param[in] format printf style format to use.
        /// \param[in] argptr Argument list.
        /// \return Formatted string.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatStringHelper (
            const char *format,
            va_list argptr);
        /// \brief
        /// Format a variable argument string.
        /// \param[in] format printf style format to use.
        /// \param[in] ... Variable number of argumets coinsiding with format.
        /// \return Formatted string.
        _LIB_THEKOGANS_UTIL_DECL std::string FormatString (
            const char *format,
            ...);
        /// \brief
        /// Format a platform specific error code.
        /// \param[in] value Platform specific error code.
        /// \return Formatted error code.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ErrorCodeTostring (
            THEKOGANS_UTIL_ERROR_CODE value);

        /// \brief
        /// Return the value of an environment variable corresponding
        /// to the given name. Empty string if no variable with the
        /// given name exists.
        /// VERY IMPORTANT: Because of the semantics of getenv,
        /// this function is not thread safe. It is therefore
        /// highly recommended that in threaded code (where multiple
        /// threads can call this function at any time) that you only
        /// use it during initialization time (before creating
        /// additional threads).
        /// \param[in] name Name of variable whose value to return.
        /// \return Value of variable, empty string if not found.
        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetEnvironmentVariable (
            const std::string &name);

        /// \brief
        /// Given an environment variable name, set it's value to the one given.
        /// \param[in] name Environment variable name whose value to set.
        /// \param[in] value Environment variable value to set.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API SetEnvironmentVariable (
            const std::string &name,
            const std::string &value);

        /// \brief
        /// Given an environment variable name, remove it from the environment.
        /// \param[in] name Environment variable name to remove.
        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API DelEnvironmentVariable (
            const std::string &name);

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_StringUtils_h)
