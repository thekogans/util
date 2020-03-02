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

#if !defined (__thekogans_util_Hash_h)
#define __thekogans_util_Hash_h

#include <cstddef>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"

namespace thekogans {
    namespace util {

        /// \struct Hash Hash.h thekogans/util/Hash.h
        ///
        /// \brief
        /// Base class used to represent an abstract hash generator.

        struct _LIB_THEKOGANS_UTIL_DECL Hash :
                public virtual ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Hash>.
            typedef ThreadSafeRefCounted::Ptr<Hash> Ptr;

            /// \brief
            /// typedef for the Hash factory function.
            typedef Ptr (*Factory) ();
            /// \brief
            /// typedef for the Hash map.
            typedef std::map<std::string, Factory> Map;
            /// \brief
            /// Controls Map's lifetime.
            /// \return Hash map.
            static Map &GetMap ();
            /// \brief
            /// Used for Hash dynamic discovery and creation.
            /// \param[in] type Hash type (it's name).
            /// \return A Hash based on the passed in type.
            static Ptr Get (const std::string &type);
            /// \struct Hash::MapInitializer Hash.h thekogans/util/Hash.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the Hash::map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_HASH/THEKOGANS_UTIL_IMPLEMENT_HASH.
            /// If you are deriving a hasher from Hash, and you want
            /// it to be dynamically discoverable/creatable, add
            /// THEKOGANS_UTIL_DECLARE_HASH to it's declaration,
            /// and THEKOGANS_UTIL_IMPLEMENT_HASH to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL MapInitializer {
                /// \brief
                /// ctor. Add hasher of type, and factory for creating it
                /// to the Hash::map
                /// \param[in] type Hash type (it's class name).
                /// \param[in] factory Hash creation factory.
                MapInitializer (
                    const std::string &type,
                    Factory factory);
            };
            /// \brief
            /// Get the list of all hashers registered with the map.
            static void GetHashers (std::list<std::string> &hashers);
        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Because Hash uses dynamic initialization, when using
            /// it in static builds call this method to have the Hash
            /// explicitly include all internal hash types. Without
            /// calling this api, the only hashers that will be available
            /// to your application are the ones you explicitly link to.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// Virtual dtor.
            virtual ~Hash () {}

            /// \brief
            /// Digest type.
            typedef std::vector<ui8> Digest;
            /// \brief
            /// Convert a given digest to it's string representation.
            /// \param[in] digest Digest to convert.
            /// \return Digest's string representation.
            static std::string DigestTostring (const Digest &digest);
            /// \brief
            /// Convert a given digest string representation to digest.
            /// \param[in] digest Digest string to convert.
            /// \return Digest.
            static Digest stringToDigest (const std::string &digest);

            /// \brief
            /// Return hasher name.
            /// \return Hasher name.
            virtual std::string GetName (std::size_t /*digestSize*/) const = 0;
            /// This API is used in streaming situations. Call Init at the
            /// beginning of the stream. As data comes in, call Update with
            /// each successive chunk. Once all data has been received,
            /// call Final to get the digest.
            /// \brief
            /// Return hasher supported digest sizes.
            /// \param[out] digestSizes List of supported digest sizes.
            virtual void GetDigestSizes (std::list<std::size_t> & /*digestSizes*/) const = 0;
            /// \brief
            /// Initialize the hasher.
            /// \param[in] digestSize digest size.
            virtual void Init (std::size_t /*digestSize*/) = 0;
            /// \brief
            /// Hash a buffer.
            /// \param[in] buffer Buffer to hash.
            /// \param[in] size Size of buffer in bytes.
            virtual void Update (
                const void * /*buffer*/,
                std::size_t /*size*/) = 0;
            /// \brief
            /// Finalize the hashing operation, and retrieve the digest.
            /// \param[out] digest Result of the hashing operation.
            virtual void Final (Digest & /*digest*/) = 0;

            /// \brief
            /// Create a digest from a given buffer.
            /// \param[in] buffer Beginning of buffer.
            /// \param[in] size Size of buffer in bytes.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromBuffer (
                const void *buffer,
                std::size_t size,
                std::size_t digestSize,
                Digest &digest);
            /// \brief
            /// Create a digest from a given file.
            /// \param[in] path File from which to generate the digest.
            /// \param[in] digestSize Size of difest in bytes.
            /// \param[out] digest Where to store the generated digest.
            void FromFile (
                const std::string &path,
                std::size_t digestSize,
                Digest &digest);
        };

        /// \brief
        /// Compare two digests for equality.
        /// \param[in] digest1 First digest to compare.
        /// \param[in] digest2 Second digest to compare.
        /// \return true = equal, false = not equal.
        inline bool operator == (
                const Hash::Digest &digest1,
                const Hash::Digest &digest2) {
            return digest1.size () == digest2.size () &&
                memcmp (&digest1[0], &digest2[0], digest1.size ()) == 0;
        }

        /// \brief
        /// Compare two digests for inequality.
        /// \param[in] digest1 First digest to compare.
        /// \param[in] digest2 Second digest to compare.
        /// \return true = not equal, false = equal.
        inline bool operator != (
                const Hash::Digest &digest1,
                const Hash::Digest &digest2) {
            return digest1.size () != digest2.size () ||
                memcmp (&digest1[0], &digest2[0], digest1.size ()) != 0;
        }

        /// \def THEKOGANS_UTIL_DECLARE_HASH_COMMON(type)
        /// Common dynamic discovery macro.
        #define THEKOGANS_UTIL_DECLARE_HASH_COMMON(type)\
        public:\
            static thekogans::util::Hash::Ptr Create () {\
                return thekogans::util::Hash::Ptr (new type);\
            }

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_HASH(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SHA1 : public Hash {
        ///     THEKOGANS_UTIL_DECLARE_HASH (SHA1)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_HASH(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_HASH_COMMON (type)\
            static void StaticInit () {\
                static volatile bool registered = false;\
                static thekogans::util::SpinLock spinLock;\
                thekogans::util::LockGuard<thekogans::util::SpinLock> guard (spinLock);\
                if (!registered) {\
                    std::pair<Map::iterator, bool> result =\
                        GetMap ().insert (Map::value_type (#type, type::Create));\
                    if (!result.second) {\
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                            "'%s' is already registered.", #type);\
                    }\
                    registered = true;\
                }\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_HASH(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_HASH (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_HASH(type)
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_HASH(type)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SHA1 : public Hash {
        ///     THEKOGANS_UTIL_DECLARE_HASH (SHA1)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_HASH(type)\
        public:\
            THEKOGANS_UTIL_DECLARE_HASH_COMMON (type)\
            static const thekogans::util::Hash::MapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_HASH(type)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// THEKOGANS_UTIL_IMPLEMENT_HASH (SHA1)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_HASH(type)\
            const thekogans::util::Hash::MapInitializer type::mapInitializer (\
                #type, type::Create);
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Hash_h)
