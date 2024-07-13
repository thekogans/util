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

#include "thekogans/util/HRTimerMgr.h"

namespace thekogans {
    namespace util {

        THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_BASE (HRTimerMgr::TimerInfoBase)

        std::size_t HRTimerMgr::TimerInfoBase::Size () const {
            return Serializer::Size (name) + Serializer::Size (attributes);
        }

        void HRTimerMgr::TimerInfoBase::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> name >> attributes;
        }

        void HRTimerMgr::TimerInfoBase::Write (Serializer &serializer) const {
            serializer << name << attributes;
        }

        const char * const HRTimerMgr::TimerInfoBase::TAG_ATTRIBUTES = "Attributes";
        const char * const HRTimerMgr::TimerInfoBase::TAG_ATTRIBUTE = "Attribute";
        const char * const HRTimerMgr::TimerInfoBase::ATTR_NAME = "Name";
        const char * const HRTimerMgr::TimerInfoBase::ATTR_VALUE = "Value";

        void HRTimerMgr::TimerInfoBase::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            name = node.attribute (ATTR_NAME).value ();
            attributes.clear ();
            pugi::xml_node attributesNode = node.child (TAG_ATTRIBUTES);
            for (pugi::xml_node child = attributesNode.first_child ();
                    !child.empty (); child = child.next_sibling ()) {
                std::string name = child.attribute (ATTR_NAME).value ();
                std::string value = child.attribute (ATTR_NAME).value ();
                if (!name.empty () && !value.empty ()) {
                    attributes.push_back (Attribute (name, value));
                }
            }
        }

        void HRTimerMgr::TimerInfoBase::Write (pugi::xml_node &node) const {
            node.append_attribute (ATTR_NAME).set_value (Encodestring (name).c_str ());
            if (!attributes.empty ()) {
                pugi::xml_node attributesNode = node.append_child (TAG_ATTRIBUTES);
                for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                    pugi::xml_node attributeNode = attributesNode.append_child (TAG_ATTRIBUTE);
                    attributeNode.append_attribute (ATTR_NAME).set_value (
                        Encodestring (attributes[i].first).c_str ());
                    attributeNode.append_attribute (ATTR_VALUE).set_value (
                        Encodestring (attributes[i].second).c_str ());
                }
            }
        }

        void HRTimerMgr::TimerInfoBase::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            name = object.Get<JSON::String> (ATTR_NAME)->value;
            util::JSON::Array::SharedPtr attributesArray = object.Get<JSON::Array> (TAG_ATTRIBUTES);
            if (attributesArray.Get () != nullptr) {
                for (std::size_t i = 0, count = attributesArray->GetValueCount (); i < count; ++i) {
                    util::JSON::Object::SharedPtr attributeObject = attributesArray->Get<util::JSON::Object> (i);
                    std::string name = attributeObject->Get<JSON::String> (ATTR_NAME)->value;
                    std::string value = attributeObject->Get<JSON::String> (ATTR_VALUE)->value;
                    if (!name.empty () && !value.empty ()) {
                        attributes.push_back (Attribute (name, value));
                    }
                }
            }
        }

        void HRTimerMgr::TimerInfoBase::Write (JSON::Object &object) const {
            object.Add<const std::string &> (ATTR_NAME, name);
            if (!attributes.empty ()) {
                util::JSON::Array::SharedPtr attributesArray (new util::JSON::Array);
                for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                    util::JSON::Object::SharedPtr attributeObject (new util::JSON::Object);
                    attributeObject->Add<const std::string &> (ATTR_NAME, attributes[i].first);
                    attributeObject->Add<const std::string &> (ATTR_VALUE, attributes[i].second);
                    attributesArray->Add (attributeObject);
                }
                object.Add (TAG_ATTRIBUTES, attributesArray);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        #if !defined (THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EX (
            HRTimerMgr::TimerInfo,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_TIMER_INFOS_IN_PAGE,
            DefaultAllocator::Instance ().Get ())

        void HRTimerMgr::TimerInfo::Start () {
            start = HRTimer::Click ();
        }

        void HRTimerMgr::TimerInfo::Stop () {
            stop = HRTimer::Click ();
        }

        namespace {
            const char * const ATTR_ELAPSED = "Elapsed";
        }

        void HRTimerMgr::TimerInfo::ToXML (pugi::xml_node &node) const {
            node.set_name (TAG_TIMER);
            node.append_attribute (ATTR_NAME).set_value (Encodestring (name).c_str ());
            node.append_attribute (ATTR_START).set_value (ui64Tostring (start).c_str ());
            node.append_attribute (ATTR_STOP).set_value (ui64Tostring (stop).c_str ());
            node.append_attribute (ATTR_ELAPSED).set_value (
                f64Tostring (
                    HRTimer::ToSeconds (
                        HRTimer::ComputeElapsedTime (start, stop))).c_str ());
            for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                node.append_attribute (attributes[i].first.c_str ()).set_value (
                    Encodestring (attributes[i].second).c_str ());
            }
        }

        void HRTimerMgr::TimerInfo::ToJSON (JSON::Object &object) const {
            object.Add<const std::string &> (ATTR_NAME, name);
            object.Add (ATTR_START, start);
            object.Add (ATTR_STOP, stop);
            object.Add<const std::string &> (ATTR_ELAPSED,
                f64Tostring (
                    HRTimer::ToSeconds (
                        HRTimer::ComputeElapsedTime (start, stop))));
            for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                object.Add<const std::string &> (
                    attributes[i].first,
                    Encodestring (attributes[i].second));
            }
        }

        void HRTimerMgr::TimerInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            count = 1;
            min = max = average = total = HRTimer::ComputeElapsedTime (start, stop);
        }

        std::size_t HRTimerMgr::TimerInfo::Size () const {
            return TimerInfoBase::Size () +
                Serializer::Size (start) +
                Serializer::Size (stop);
        }

        void HRTimerMgr::TimerInfo::Read (
                const BinHeader &header,
                Serializer &serializer) {
            TimerInfoBase::Read (header, serializer);
            serializer >> start >> stop;
        }

        void HRTimerMgr::TimerInfo::Write (Serializer &serializer) const {
            TimerInfoBase::Write (serializer);
            serializer << start << stop;
        }

        const char * const HRTimerMgr::TimerInfo::TAG_TIMER = "Timer";
        const char * const HRTimerMgr::TimerInfo::ATTR_START = "Start";
        const char * const HRTimerMgr::TimerInfo::ATTR_STOP = "Stop";

        void HRTimerMgr::TimerInfo::Read (
                const TextHeader &header,
                const pugi::xml_node &node) {
            TimerInfoBase::Read (header, node);
            start = stringToui64 (node.attribute (ATTR_START).value ());
            stop = stringToui64 (node.attribute (ATTR_STOP).value ());
        }

        void HRTimerMgr::TimerInfo::Write (pugi::xml_node &node) const {
            TimerInfoBase::Write (node);
            node.append_attribute (ATTR_START).set_value (ui64Tostring (start).c_str ());
            node.append_attribute (ATTR_STOP).set_value (ui64Tostring (stop).c_str ());
        }

        void HRTimerMgr::TimerInfo::Read (
                const TextHeader &header,
                const JSON::Object &object) {
            TimerInfoBase::Read (header, object);
            start = object.Get<JSON::Number> (ATTR_START)->To<ui64> ();
            stop = object.Get<JSON::Number> (ATTR_STOP)->To<ui64> ();
        }

        void HRTimerMgr::TimerInfo::Write (JSON::Object &object) const {
            TimerInfoBase::Write (object);
            object.Add (ATTR_START, start);
            object.Add (ATTR_STOP, stop);
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        #if !defined (THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE 64
        #endif // !defined (THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EX (
            HRTimerMgr::ScopeInfo,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_SCOPE_INFOS_IN_PAGE,
            DefaultAllocator::Instance ().Get ())

        HRTimerMgr::ScopeInfo *HRTimerMgr::ScopeInfo::BeginScope (
                const std::string &name) {
            SharedPtr scope (new ScopeInfo (name));
            open.push_back (dynamic_refcounted_sharedptr_cast<TimerInfoBase> (scope));
            return scope.Get ();
        }

        void HRTimerMgr::ScopeInfo::EndScope () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

        HRTimerMgr::TimerInfo *HRTimerMgr::ScopeInfo::StartTimer (
                const std::string &name) {
            TimerInfo::SharedPtr timer (new TimerInfo (name));
            open.push_back (dynamic_refcounted_sharedptr_cast<TimerInfoBase> (timer));
            return timer.Get ();
        }

        void HRTimerMgr::ScopeInfo::StopTimer () {
            if (!open.empty ()) {
                closed.push_back (open.back ());
                open.pop_back ();
            }
        }

        namespace {
            const char * const TAG_SCOPE = "Scope";
            const char * const ATTR_COUNT = "Count";
            const char * const ATTR_MIN = "Min";
            const char * const ATTR_MAX = "Max";
            const char * const ATTR_AVERAGE = "Average";
            const char * const ATTR_TOTAL = "Total";
        }

        void HRTimerMgr::ScopeInfo::ToXML (pugi::xml_node &node) const {
            ui32 count = 0;
            ui64 min = UI64_MAX;
            ui64 max = 0;
            ui64 average = 0;
            ui64 total = 0;
            GetStats (count, min, max, average, total);
            node.set_name (TAG_SCOPE);
            node.append_attribute (ATTR_NAME).set_value (Encodestring (name).c_str ());
            node.append_attribute (ATTR_COUNT).set_value (ui32Tostring (count).c_str ());
            node.append_attribute (ATTR_MIN).set_value (f64Tostring (HRTimer::ToSeconds (min)).c_str ());
            node.append_attribute (ATTR_MAX).set_value (f64Tostring (HRTimer::ToSeconds (max)).c_str ());
            node.append_attribute (ATTR_AVERAGE).set_value (f64Tostring (HRTimer::ToSeconds (average)).c_str ());
            node.append_attribute (ATTR_TOTAL).set_value (f64Tostring (HRTimer::ToSeconds (total)).c_str ());
            for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                node.append_attribute (attributes[i].first.c_str ()).set_value (
                    Encodestring (attributes[i].second).c_str ());
            }
            if (!closed.empty ()) {
                pugi::xml_node scopes = node.append_child (TAG_CLOSED_SCOPES);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    pugi::xml_node scope = scopes.append_child (pugi::node_element);
                    (*it)->ToXML (scope);
                }
            }
        }

        void HRTimerMgr::ScopeInfo::ToJSON (JSON::Object &object) const {
            ui32 count = 0;
            ui64 min = UI64_MAX;
            ui64 max = 0;
            ui64 average = 0;
            ui64 total = 0;
            GetStats (count, min, max, average, total);
            object.Add<const std::string &> (ATTR_NAME, name);
            object.Add (ATTR_COUNT, count);
            object.Add (ATTR_MIN, HRTimer::ToSeconds (min));
            object.Add (ATTR_MAX, HRTimer::ToSeconds (max));
            object.Add (ATTR_AVERAGE, HRTimer::ToSeconds (average));
            object.Add (ATTR_TOTAL, HRTimer::ToSeconds (total));
            for (std::size_t i = 0, count = attributes.size (); i < count; ++i) {
                object.Add<const std::string &> (attributes[i].first, attributes[i].second);
            }
            if (!closed.empty ()) {
                JSON::Array::SharedPtr scopes (new JSON::Array);
                object.Add (TAG_CLOSED_SCOPES, scopes);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    JSON::Object::SharedPtr scope (new JSON::Object);
                    (*it)->ToJSON (*scope);
                    scopes->Add (scope);
                }
            }
        }

        void HRTimerMgr::ScopeInfo::GetStats (
                ui32 &count,
                ui64 &min,
                ui64 &max,
                ui64 &average,
                ui64 &total) const {
            for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                    end = closed.end (); it != end; ++it) {
                ++count;
                ui32 itemCount = 0;
                ui64 itemMin = UI64_MAX;
                ui64 itemMax = 0;
                ui64 itemAverage = 0;
                ui64 itemTotal = 0;
                (*it)->GetStats (itemCount, itemMin, itemMax, itemAverage, itemTotal);
                if (min > itemMin) {
                    min = itemMin;
                }
                if (max < itemMax) {
                    max = itemMax;
                }
                average += itemAverage;
                total += itemTotal;
            }
            if (count > 0) {
                average /= count;
            }
        }

        std::size_t HRTimerMgr::ScopeInfo::Size () const {
            std::size_t size = TimerInfoBase::Size ();
            {
                size += util::SizeT (open.size ()).Size ();
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = open.begin (),
                        end = open.end (); it != end; ++it) {
                    size += (*it)->Size ();
                }
            }
            {
                size += util::SizeT (closed.size ()).Size ();
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    size += (*it)->Size ();
                }
            }
            return size;
        }

        void HRTimerMgr::ScopeInfo::Read (
                const BinHeader &header,
                Serializer &serializer) {
            TimerInfoBase::Read (header, serializer);
            {
                util::SizeT openCount;
                serializer >> openCount;
                open.clear ();
                while (openCount-- > 0) {
                    TimerInfoBase::SharedPtr openInfo;
                    serializer >> openInfo;
                    open.push_back (openInfo);
                }
            }
            {
                util::SizeT closedCount;
                serializer >> closedCount;
                closed.clear ();
                while (closedCount-- > 0) {
                    TimerInfoBase::SharedPtr closedInfo;
                    serializer >> closedInfo;
                    closed.push_back (closedInfo);
                }
            }
        }

        void HRTimerMgr::ScopeInfo::Write (Serializer &serializer) const {
            TimerInfoBase::Write (serializer);
            {
                serializer << util::SizeT (open.size ());
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = open.begin (),
                        end = open.end (); it != end; ++it) {
                    serializer << **it;
                }
            }
            {
                serializer << util::SizeT (closed.size ());
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    serializer << **it;
                }
            }
        }

        const char * const HRTimerMgr::ScopeInfo::TAG_SCOPE = "Scope";
        const char * const HRTimerMgr::ScopeInfo::TAG_OPEN_SCOPES = "OpenScopes";
        const char * const HRTimerMgr::ScopeInfo::TAG_CLOSED_SCOPES = "ClosedScopes";

        void HRTimerMgr::ScopeInfo::Read (
                const TextHeader &header,
                const pugi::xml_node &node) {
            TimerInfoBase::Read (header, node);
            {
                open.clear ();
                pugi::xml_node openScopes = node.child (TAG_OPEN_SCOPES);
                for (pugi::xml_node child = openScopes.first_child ();
                        !child.empty (); child = child.next_sibling ()) {
                    if (child.type () == pugi::node_element) {
                        std::string childName = child.name ();
                        if (childName == TAG_SCOPE) {
                            TimerInfoBase::SharedPtr timerInfo;
                            child >> timerInfo;
                            open.push_back (timerInfo);
                        }
                    }
                }
            }
            {
                closed.clear ();
                pugi::xml_node closedScopes = node.child (TAG_CLOSED_SCOPES);
                for (pugi::xml_node child = closedScopes.first_child ();
                        !child.empty (); child = child.next_sibling ()) {
                    if (child.type () == pugi::node_element) {
                        std::string childName = child.name ();
                        if (childName == TAG_SCOPE) {
                            TimerInfoBase::SharedPtr timerInfo;
                            child >> timerInfo;
                            closed.push_back (timerInfo);
                        }
                    }
                }
            }
        }

        void HRTimerMgr::ScopeInfo::Write (pugi::xml_node &node) const {
            TimerInfoBase::Write (node);
            {
                pugi::xml_node openScopes = node.append_child (TAG_OPEN_SCOPES);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = open.begin (),
                        end = open.end (); it != end; ++it) {
                    pugi::xml_node openScope = openScopes.append_child (TAG_SCOPE);
                    openScope << **it;
                }
            }
            {
                pugi::xml_node closedScopes = node.append_child (TAG_CLOSED_SCOPES);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    pugi::xml_node closedScope = closedScopes.append_child (TAG_SCOPE);
                    closedScope << **it;
                }
            }
        }

        void HRTimerMgr::ScopeInfo::Read (
                const TextHeader &header,
                const JSON::Object &object) {
            TimerInfoBase::Read (header, object);
            {
                open.clear ();
                util::JSON::Array::SharedPtr openScopes =
                    object.Get<util::JSON::Array> (TAG_OPEN_SCOPES);
                if (openScopes.Get () != nullptr) {
                    for (std::size_t i = 0, count = openScopes->GetValueCount (); i < count; ++i) {
                        util::JSON::Object::SharedPtr openScope = openScopes->Get<util::JSON::Object> (i);
                        TimerInfoBase::SharedPtr timerInfo;
                        *openScope >> timerInfo;
                        open.push_back (timerInfo);
                    }
                }
            }
            {
                closed.clear ();
                util::JSON::Array::SharedPtr closedScopes =
                    object.Get<util::JSON::Array> (TAG_CLOSED_SCOPES);
                if (closedScopes.Get () != nullptr) {
                    for (std::size_t i = 0, count = closedScopes->GetValueCount (); i < count; ++i) {
                        util::JSON::Object::SharedPtr closedScope = closedScopes->Get<util::JSON::Object> (i);
                        TimerInfoBase::SharedPtr timerInfo;
                        *closedScope >> timerInfo;
                        closed.push_back (timerInfo);
                    }
                }
            }
        }

        void HRTimerMgr::ScopeInfo::Write (JSON::Object &object) const {
            TimerInfoBase::Write (object);
            {
                util::JSON::Array::SharedPtr openScopes (new util::JSON::Array);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = open.begin (),
                        end = open.end (); it != end; ++it) {
                    util::JSON::Object::SharedPtr openScope (new util::JSON::Object);
                    *openScope << **it;
                    openScopes->Add (openScope);
                }
                object.Add (TAG_OPEN_SCOPES, openScopes);
            }
            {
                util::JSON::Array::SharedPtr closedScopes (new util::JSON::Array);
                for (std::list<TimerInfoBase::SharedPtr>::const_iterator it = closed.begin (),
                        end = closed.end (); it != end; ++it) {
                    util::JSON::Object::SharedPtr closedScope (new util::JSON::Object);
                    *closedScope << **it;
                    closedScopes->Add (closedScope);
                }
                object.Add (TAG_CLOSED_SCOPES, closedScopes);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////

        #if !defined (THEKOGANS_UTIL_MIN_HR_TIMER_MGR_IN_PAGE)
            #define THEKOGANS_UTIL_MIN_HR_TIMER_MGR_IN_PAGE 16
        #endif // !defined (THEKOGANS_UTIL_MIN_HR_TIMER_MGR_IN_PAGE)

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EX (
            HRTimerMgr,
            1,
            SpinLock,
            THEKOGANS_UTIL_MIN_HR_TIMER_MGR_IN_PAGE,
            DefaultAllocator::Instance ().Get ())


        std::string HRTimerMgr::ToXMLString (
                std::size_t indentationLevel,
                std::size_t indentationWidth) const {
            pugi::xml_document document;
            pugi::xml_node node = document.append_child (TAG_SCOPE);
            root.ToXML (node);
            return FormatDocument (document, indentationLevel, indentationWidth);
        }

        std::string HRTimerMgr::ToJSONString (
                std::size_t indentationLevel,
                std::size_t indentationWidth) const {
            JSON::Object object;
            root.ToJSON (object);
            return JSON::FormatValue (object, indentationLevel, indentationWidth);
        }

        std::size_t HRTimerMgr::Size () const {
            return root.Size ();
        }

        void HRTimerMgr::Read (
                const BinHeader & /*header*/,
                Serializer &serializer) {
            serializer >> root;
        }

        void HRTimerMgr::Write (Serializer &serializer) const {
            serializer << root;
        }

        void HRTimerMgr::Read (
                const TextHeader & /*header*/,
                const pugi::xml_node &node) {
            node >> root;
        }

        void HRTimerMgr::Write (pugi::xml_node &node) const {
            node << root;
        }

        void HRTimerMgr::Read (
                const TextHeader & /*header*/,
                const JSON::Object &object) {
            object >> root;
        }

        void HRTimerMgr::Write (JSON::Object &object) const {
            object << root;
        }

    } // namespace util
} // namespace thekogans
