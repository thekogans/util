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

#include "thekogans/util/Exception.h"
#include "thekogans/util/SHA2.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_HASH (SHA2)

        void SHA2::GetDigestSizes (std::list<ui32> &digestSizes) const {
            digestSizes.push_back (DIGEST_SIZE_224);
            digestSizes.push_back (DIGEST_SIZE_256);
            digestSizes.push_back (DIGEST_SIZE_384);
            digestSizes.push_back (DIGEST_SIZE_512);
        }

        void SHA2::Init (ui32 digestSize_) {
            digestSize = digestSize_;
            switch (digestSize) {
                case DIGEST_SIZE_224:
                case DIGEST_SIZE_256:
                    hasher_224_256.Init (digestSize);
                    break;
                case DIGEST_SIZE_384:
                case DIGEST_SIZE_512:
                    hasher_384_512.Init (digestSize);
                    break;
                default:
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        void SHA2::Update (
                const void *buffer,
                std::size_t size) {
            switch (digestSize) {
                case DIGEST_SIZE_224:
                case DIGEST_SIZE_256:
                    hasher_224_256.Update (buffer, size);
                    break;
                case DIGEST_SIZE_384:
                case DIGEST_SIZE_512:
                    hasher_384_512.Update (buffer, size);
                    break;
                default:
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unsupported digest size: %u", digestSize);
            }
        }

        void SHA2::Final (Digest &digest) {
            switch (digestSize) {
                case DIGEST_SIZE_224:
                case DIGEST_SIZE_256:
                    hasher_224_256.Final (digest);
                    break;
                case DIGEST_SIZE_384:
                case DIGEST_SIZE_512:
                    hasher_384_512.Final (digest);
                    break;
                default:
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Unsupported digest size: %u", digestSize);
            }
        }

    } // namespace util
} // namespace thekogans
