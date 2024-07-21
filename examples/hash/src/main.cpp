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
#include <iostream>
#include <list>
#include "thekogans/util/OwnerList.h"
#include "thekogans/util/CommandLineOptions.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/ConsoleLogger.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/Hash.h"

using namespace thekogans;

namespace {
    std::string GetHasherList (const std::string &separator) {
        std::string hasherList;
        {
            std::list<std::string> hashers;
            util::Hash::GetTypes (hashers);
            if (!hashers.empty ()) {
                std::list<std::string>::const_iterator it = hashers.begin ();
                hasherList = *it++;
                for (std::list<std::string>::const_iterator end = hashers.end (); it != end; ++it) {
                    hasherList += separator + *it;
                }
            }
            else {
                hasherList = "No hashers defined";
            }
        }
        return hasherList;
    }
}

int main (
        int argc,
        const char *argv[]) {
#if defined (TOOLCHAIN_TYPE_Static)
    util::Hash::StaticInit ();
#endif // defined (TOOLCHAIN_TYPE_Static)
    struct Options : public util::CommandLineOptions {
        struct Hasher {
            std::string name;
            std::list<std::size_t> digestSizes;
            Hasher (const std::string &name_) :
                name (name_) {}
        };
        util::OwnerList<Hasher> hashers;
        bool naked;
        std::string path;

        Options () :
            naked (false) {}

        virtual void DoOption (
                char option,
                const std::string &value) {
            switch (option) {
                case 'h': {
                    if (!hashers.empty () && hashers.back ()->digestSizes.empty ()) {
                        util::Hash::SharedPtr hash =
                            util::Hash::CreateType (hashers.back ()->name);
                        assert (hash.Get () != 0);
                        hashers.back ()->digestSizes.clear ();
                        hash->GetDigestSizes (hashers.back ()->digestSizes);
                    }
                    util::Hash::SharedPtr hash = util::Hash::CreateType (value);
                    if (hash.Get () != 0) {
                        hashers.push_back (new Hasher (value));
                    }
                    else {
                        std::cout << "Unable to get hasher: " << value << ", skipping.";
                    }
                    break;
                }
                case 'd': {
                    if (!hashers.empty ()) {
                        if (value == "ALL") {
                            util::Hash::SharedPtr hash =
                                util::Hash::CreateType (hashers.back ()->name);
                            assert (hash.Get () != 0);
                            hashers.back ()->digestSizes.clear ();
                            hash->GetDigestSizes (hashers.back ()->digestSizes);
                            if (hashers.back ()->digestSizes.empty ()) {
                                std::cout << "Hasher: " << hash->GetDigestName (0) <<
                                    " does not expose digests, skipping.";
                                delete hashers.back ();
                                hashers.pop_back ();
                            }
                        }
                        else {
                            // FIXME: check for duplicates, and if supported.
                            hashers.back ()->digestSizes.push_back (
                                util::stringToui32 (value.c_str ()) / 8);
                        }
                    }
                    else {
                        std::cout << "-d:" << value << " appears out of place, skipping.";
                    }
                    break;
                }
                case 'n': {
                    naked = true;
                    break;
                }
            }
        }
        virtual void DoPath (const std::string &value) {
            path = value;
        }
        virtual void Epilog () {
            if (!hashers.empty () && hashers.back ()->digestSizes.empty ()) {
                util::Hash::SharedPtr hash = util::Hash::CreateType (hashers.back ()->name);
                assert (hash.Get () != 0);
                hashers.back ()->digestSizes.clear ();
                hash->GetDigestSizes (hashers.back ()->digestSizes);
            }
        }
    } options;
    options.Parse (argc, argv, "hdn");
    if (options.hashers.empty () || options.path.empty ()) {
        std::cout << "usage: " << argv[0] <<
            " -h:[" << GetHasherList (" | ") << "] -d:[digestSize | ALL]... [-n] path" << std::endl;
        return 1;
    }
    THEKOGANS_UTIL_LOG_INIT (
        util::LoggerMgr::Debug,
        util::LoggerMgr::All);
    THEKOGANS_UTIL_LOG_ADD_LOGGER (util::Logger::SharedPtr (new util::ConsoleLogger));
    THEKOGANS_UTIL_IMPLEMENT_LOG_FLUSHER;
    for (util::OwnerList<Options::Hasher>::const_iterator
            it = options.hashers.begin (),
            end = options.hashers.end (); it != end; ++it) {
        util::Hash::SharedPtr hash = util::Hash::CreateType ((*it)->name);
        assert (hash.Get () != 0);
        for (std::list<std::size_t>::const_iterator
                jt = (*it)->digestSizes.begin (),
                end = (*it)->digestSizes.end (); jt != end; ++jt) {
            THEKOGANS_UTIL_TRY {
                util::Hash::Digest digest;
                hash->FromFile (options.path, *jt, digest);
                if (!options.naked) {
                    std::cout << hash->GetDigestName (*jt)  << ": " <<
                        util::Hash::DigestTostring (digest) << std::endl;
                }
                else {
                    std::cout << util::Hash::DigestTostring (digest);
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG
        }
    }
    return 0;
}
