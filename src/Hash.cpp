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

#include <cassert>
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/File.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Hash.h"
#if defined (THEKOGANS_UTIL_TYPE_Static)
    #include "thekogans/util/MD5.h"
    #include "thekogans/util/SHA1.h"
    #include "thekogans/util/SHA2.h"
    #include "thekogans/util/SHA3.h"
#endif // defined (THEKOGANS_UTIL_TYPE_Static)

namespace thekogans {
    namespace util {

        Hash::Map &Hash::GetMap () {
            static Hash::Map map;
            return map;
        }

        Hash::Ptr Hash::Get (const std::string &type) {
            Map::iterator it = GetMap ().find (type);
            return it != GetMap ().end () ?
                it->second () : Hash::Ptr ();
        }

        Hash::MapInitializer::MapInitializer (
                const std::string &type,
                Factory factory) {
            std::pair<Map::iterator, bool> result =
                GetMap ().insert (Map::value_type (type, factory));
            assert (result.second);
            if (!result.second) {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s is already registered.", type.c_str ());
            }
        }

        void Hash::GetHashers (std::list<std::string> &hashers) {
            for (Map::const_iterator it = GetMap ().begin (),
                    end = GetMap ().end (); it != end; ++it) {
                hashers.push_back (it->first);
            }
        }

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        void Hash::StaticInit () {
            static volatile bool registered = false;
            static SpinLock spinLock;
            LockGuard<SpinLock> guard (spinLock);
            if (!registered) {
                MD5::StaticInit ();
                SHA1::StaticInit ();
                SHA2::StaticInit ();
                SHA3::StaticInit ();
                registered = true;
            }
        }
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        std::string Hash::DigestTostring (const Digest &digest) {
            return HexEncodeBuffer (digest.data (), digest.size ());
        }

        Hash::Digest Hash::stringToDigest (const std::string &digest) {
            return HexDecodeBuffer (digest.c_str (), digest.size ());
        }

        void Hash::FromBuffer (
                const void *buffer,
                std::size_t size,
                std::size_t digestSize,
                Digest &digest) {
            if (buffer != 0 && size > 0) {
                Init (digestSize);
                Update (buffer, size);
                Final (digest);
            }
            else {
                digest.clear ();
            }
        }

        void Hash::FromFile (
                const std::string &path,
                std::size_t digestSize,
                Digest &digest) {
            ReadOnlyFile file (HostEndian, path);
            if (file.GetSize () > 0) {
                Init (digestSize);
                ui8 buffer[4096];
                for (std::size_t size = file.Read (buffer, 4096);
                        size != 0; size = file.Read (buffer, 4096)) {
                    Update (buffer, size);
                }
                Final (digest);
            }
            else {
                digest.clear ();
            }
        }

    } // namespace util
} // namespace thekogans
