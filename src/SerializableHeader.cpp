// Copyright 2016 Boris Kogan (boris@thekogans.net)
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

#include "thekogans/util/XMLUtils.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/Serializable.h"
#include "thekogans/util/SerializableHeader.h"

namespace thekogans {
    namespace util {

        void ValueParser<SerializableHeader>::Reset (Serializer &serializer) {
            idParser.Reset ();
            typeParser.Reset ();
            versionParser.Reset ();
            sizeParser.Reset ();
            value = serializer.context;
            if (value.NeedType ()) {
                if (serializer.GetTypeDictionary () != nullptr) {
                    state = STATE_ID;
                }
                else {
                    state = STATE_TYPE;
                }
            }
            else if (value.NeedVersion ()) {
                state = STATE_VERSION;
            }
            else if (value.NeedSize ()) {
                state = STATE_SIZE;
            }
            else {
                state = STATE_NOTHING;
            }
        }

        bool ValueParser<SerializableHeader>::ParseValue (Serializer &serializer) {
            if (state == STATE_NOTHING) {
                Reset (serializer);
                return true;
            }
            if (state == STATE_ID) {
                if (idParser.ParseValue (serializer)) {
                    value.type = serializer.GetTypeDictionary ()->ResolveTypeId (id);
                    if (value.NeedVersion ()) {
                        state = STATE_VERSION;
                    }
                    else if (value.NeedSize ()) {
                        state = STATE_SIZE;
                    }
                    else {
                        Reset (serializer);
                        return true;
                    }
                }
            }
            if (state == STATE_TYPE) {
                if (typeParser.ParseValue (serializer)) {
                    value.type = Serializable::TypeForType (type.c_str ());
                    if (value.NeedVersion ()) {
                        state = STATE_VERSION;
                    }
                    else if (value.NeedSize ()) {
                        state = STATE_SIZE;
                    }
                    else {
                        Reset (serializer);
                        return true;
                    }
                }
            }
            if (state == STATE_VERSION) {
                if (versionParser.ParseValue (serializer)) {
                    if (value.NeedSize ()) {
                        state = STATE_SIZE;
                    }
                    else {
                        Reset (serializer);
                        return true;
                    }
                }
            }
            if (state == STATE_SIZE) {
                if (sizeParser.ParseValue (serializer)) {
                    Reset (serializer);
                    return true;
                }
            }
            return false;
        }

    } // namespace util
} // namespace thekogans
