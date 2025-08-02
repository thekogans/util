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

#if !defined (__thekogans_util_Serializable_h)
#define __thekogans_util_Serializable_h

#include <cstddef>
#include <string>
#include <ostream>
#include "pugixml/pugixml.hpp"
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/Constants.h"
#include "thekogans/util/DynamicCreatable.h"
#include "thekogans/util/Serializer.h"
#include "thekogans/util/JSON.h"
#include "thekogans/util/Buffer.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/ValueParser.h"

namespace thekogans {
    namespace util {

        /// \struct Serializable Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Serializable extends \see{DynamicCreatable} to provide object storage/retrieval
        /// facilities for three distinct protocols; binary, XML and JSON. It is an abstract
        /// base for all supported serializable types and exposes machinery used by descendants
        /// to register themselves for dynamic discovery, creation and serializable insertion and
        /// extraction. Serializable has built in support for binary, XML and JSON
        /// serialization and de-serialization. (For a good real world example have a
        /// look at \see{crypto::Serializable} and it's derivatives.)
        struct _LIB_THEKOGANS_UTIL_DECL Serializable : public DynamicCreatable {
            /// \brief
            /// Serializable is a \see{util::DynamicCreatable} abstract base.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_ABSTRACT_BASE (Serializable)

            /// \struct Serializable::Header Serializable.h thekogans/util/Serializable.h
            ///
            /// \brief
            /// Header containing enough info to deserialize the serializable instance.
            struct _LIB_THEKOGANS_UTIL_DECL Header {
                /// \brief
                /// Serializable type (it's class name).
                std::string type;
                /// \brief
                /// Serializable version.
                ui16 version;
                /// \brief
                /// Serializable size in bytes (not including the header).
                SizeT size;

                /// \brief
                /// ctor.
                Header () :
                    version (0),
                    size (0) {}
                /// \brief
                /// ctor.
                /// \param[in] type_ Serializable type (it's class name).
                /// \param[in] version_ Serializable version.
                /// \param[in] size_ Serializable size in bytes (not including the header).
                Header (
                    const std::string &type_,
                    ui16 version_,
                    std::size_t size_) :
                    type (type_),
                    version (version_),
                    size (size_) {}

                /// \brief
                /// "Type"
                static const char * const ATTR_TYPE;
                /// \brief
                /// "Version"
                static const char * const ATTR_VERSION;

                /// \brief
                /// Return the binary header size.
                /// \return Header binary size.
                inline std::size_t Size () const {
                    return
                        UI32_SIZE +
                        Serializer::Size (type) +
                        Serializer::Size (version) +
                        Serializer::Size (size);
                }
            };

        #if defined (THEKOGANS_UTIL_TYPE_Static)
            /// \brief
            /// Register all known bases. This method is meant to be added
            /// to as new Serializable derivatives are added to the system.
            static void StaticInit ();
        #endif // defined (THEKOGANS_UTIL_TYPE_Static)

            /// \brief
            /// Return the binary size of the serializable including the header.
            /// \return Size of the binary serializable including the header.
            std::size_t GetSize () const noexcept;

            /// \brief
            /// Serializable objects come in to existance in one of two ways.
            /// Either their shell is created and the contents are read from
            /// \see{Serializer}, or you create one explicitly and pass the
            /// parameters to a ctor. The first case must defer initialization
            /// until after the object is read. To accomodate both cases use
            /// this methdod to do all object initialization. Serializable
            /// machinery calls it after object extraction automatically. You
            /// can call it in your ctor after initializing the members with
            /// the passed in values.
            virtual void Init () {}

            /// \brief
            /// Return the serializable version.
            /// \return Serializable version.
            virtual ui16 Version () const noexcept = 0;

            /// \brief
            /// Return the serializable binary size (not including the header).
            /// \return Serializable binary size.
            virtual std::size_t Size () const noexcept = 0;

            /// \brief
            /// Write the serializable from the given serializer.
            /// \param[in] header
            /// \param[in] serializer Serializer to read the serializable from.
            virtual void Read (
                const Header & /*header*/,
                Serializer & /*serializer*/) = 0;
            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Write (Serializer & /*serializer*/) const = 0;

            /// \brief
            /// Read the Serializable from an XML DOM.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void ReadXML (
                const Header & /*header*/,
                const pugi::xml_node & /*node*/) = 0;
            /// \brief
            /// Write the Serializable to the XML DOM.
            /// \param[out] node Parent node.
            virtual void WriteXML (pugi::xml_node & /*node*/) const = 0;

            /// \brief
            /// Read the Serializable from an JSON DOM.
            /// \param[in] node JSON DOM representation of a Serializable.
            virtual void ReadJSON (
                const Header & /*header*/,
                const JSON::Object & /*object*/) = 0;
            /// \brief
            /// Write a Serializable to the JSON DOM.
            /// \param[out] node Parent node.
            virtual void WriteJSON (JSON::Object & /*object*/) const = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_VERSION(_T)
        /// Common defines for Serializable.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_VERSION(_T)\
        public:\
            static const thekogans::util::ui16 VERSION;

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VERSION(_T, version)
        /// Serializable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VERSION(_T, version)\
            const thekogans::util::ui16 _T::VERSION = version;

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_Version(_T)
        /// Common defines for Serializable.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_Version(_T)\
        public:\
            virtual thekogans::util::ui16 Version () const noexcept override;

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_Version(_T)
        /// Serializable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_Version(_T)\
            thekogans::util::ui16 _T::Version () const noexcept {\
                return VERSION;\
            }

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_OVERRIDE(_T)
        /// Common defines for Serializable.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_OVERRIDE(_T)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_VERSION(_T)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_Version(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE(_T, version)
        /// Serializable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE(_T, version)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VERSION(_T, version)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_Version(_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE(_T, version)
        /// Serializable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE_T(_T, version)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VERSION(_T, version)\
            template<>\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_Version(_T)

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE(_T)
        /// Serializable declaration macro. Instantiate one of these
        /// in your class declaration. Ex;
        ///
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         struct _LIB_THEKOGANS_UTIL_DECL TimeSpec : public Serializable {
        ///             /// \brief
        ///             /// TimeSpec is a \see{Serializable}.
        ///             THEKOGANS_UTIL_DECLARE_SERIALIZABLE (TimeSpec)
        ///             ...
        ///         };
        ///
        ///     } // namespace util
        /// } // namespace thekogans
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE(_T)\
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE (_T)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_OVERRIDE (_T)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(_T, version, ...)
        /// Serializable implementation macro. Instantiate one of these
        /// in your class cpp. Ex;
        ///
        /// namespace thekogans {
        ///     namespace util {
        ///
        ///         THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (thekogans::util::TimeSpec, 1)
        ///
        ///     } // namespace util
        /// } // namespace thekogans
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(_T, version, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE (_T,\
                thekogans::util::Serializable::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE (_T, version)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_S(_T, version, ...)
        /// Serializable implementation macro. Instantiate one of these
        /// in your \see{Singleton} derived class cpp.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_S(_T, version, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (_T,\
                thekogans::util::Serializable::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE (_T, version)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_T(_T, version, ...)
        /// Serializable implementation macro. Instantiate one of these
        /// in your template instantiation class cpp.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_T(_T, version, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_T (_T,\
                thekogans::util::Serializable::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE_T (_T, version)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_ST(_T, version, ...)
        /// Serializable implementation macro. Instantiate one of these
        /// in your \see{Singleton},  template instantiation class cpp.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_ST(_T, version, ...)\
            THEKOGANS_UTIL_IMPLEMENT_DYNAMIC_CREATABLE_S (_T,\
                thekogans::util::Serializable::TYPE, ##__VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE_T (_T, version)

        /// \struct Blob Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Blob is a standin for unrecognized types. Serializable is designed to
        /// marshal \see{DynamicCreatable} structured types. During marshaling the
        /// code might come accross a type that has not been registered. Instead of
        /// ignoring the data and potentially loosing information or throwing
        /// exceptions, Blob is used to contain unstructured bits. Blob is designed
        /// to put the bits back exactly as it found them. The limitation is that if
        /// you made a binary blob, you cannot store it as an XML or JSON blob. That
        /// kind of conversion requires knowledge of the underlying type.

        struct _LIB_THEKOGANS_UTIL_DECL Blob : public Serializable {
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Blob)
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Type (Blob)
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASES (Blob)
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_Bases (Blob)

            /// \brief
            /// Blob type.
            Header header;
            /// \brief
            /// Binary blob.
            Buffer buffer;
            /// \brief
            /// XML blob.
            pugi::xml_node node;
            /// \brief
            /// JSON blob.
            JSON::Object object;

            /// \brief
            /// Return the serializable version.
            /// \return Serializable version.
            virtual ui16 Version () const noexcept override;

            /// \brief
            /// Return the serializable size (not including the header).
            /// \return Serializable size.
            virtual std::size_t Size () const noexcept override;

            /// \brief
            /// Write the serializable from the given serializer.
            /// \param[in] header_ Serializable::Header to deserialize.
            /// \param[in] serializer Serializer to read the serializable from.
            virtual void Read (
                const Header &header_,
                Serializer &serializer) override;
            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Write (Serializer &serializer) const override;

            /// \brief
            /// Read the Serializable from an XML DOM.
            /// \param[in] header_ Serializable::Header to deserialize.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void ReadXML (
                const Header &header_,
                const pugi::xml_node &node_) override;
            /// \brief
            /// Write a Serializable to the XML DOM.
            /// \param[out] node_ Parent node.
            virtual void WriteXML (pugi::xml_node &node_) const override;

            /// \brief
            /// Read the Serializable from an JSON DOM.
            /// \param[in] header_ Serializable::Header to deserialize.
            /// \param[in] object_ JSON DOM representation of a Serializable.
            virtual void ReadJSON (
                const Header &header_,
                const JSON::Object &object_) override;
            /// \brief
            /// Write a Serializable to the JSON DOM.
            /// \param[out] object_ Parent node.
            virtual void WriteJSON (JSON::Object &object_) const override;
        };

        /// \brief
        /// Serializable::Header insertion operator.
        /// \param[in] serializer Where to serialize the serializable header.
        /// \param[in] header Serializable::Header to serialize.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Serializable::Header &header) {
            serializer << MAGIC32 << header.type << header.version << header.size;
            return serializer;
        }

        /// \brief
        /// Serializable::Header extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::Header to deserialize.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable::Header &header) {
            ui32 magic;
            serializer >> magic;
            if (magic == MAGIC32) {
                serializer >> header.type >> header.version >> header.size;
                return serializer;
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Corrupt serializable header: %u.",
                    magic);
            }
        }

        /// \brief
        /// Serializable::Header insertion operator.
        /// \param[in] node Where to serialize the serializable header.
        /// \param[in] header Serializable::Header to serialize.
        /// \return node.
        inline pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Serializable::Header &header) {
            node.append_attribute (
                Serializable::Header::ATTR_TYPE).set_value (header.type.c_str ());
            node.append_attribute (
                Serializable::Header::ATTR_VERSION).set_value (
                    ui32Tostring (header.version).c_str ());
            return node;
        }

        /// \brief
        /// Serializable::Header extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::Header to deserialize.
        /// \return node.
        inline const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable::Header &header) {
            header.type = node.attribute (Serializable::Header::ATTR_TYPE).value ();
            header.version = stringToui16 (
                node.attribute (Serializable::Header::ATTR_VERSION).value ());
            return node;
        }

        /// \brief
        /// Serializable::Header insertion operator.
        /// \param[in] object Where to serialize the serializable header.
        /// \param[in] header Serializable::Header to serialize.
        /// \return node.
        inline JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const Serializable::Header &header) {
            object.Add<const std::string &> (
                Serializable::Header::ATTR_TYPE,
                header.type);
            object.Add (
                Serializable::Header::ATTR_VERSION,
                header.version);
            return object;
        }

        /// \brief
        /// Serializable::Header extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::Header to deserialize.
        /// \return node.
        inline const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable::Header &header) {
            header.type = object.Get<JSON::String> (
                Serializable::Header::ATTR_TYPE)->value;
            header.version = object.Get<JSON::Number> (
                Serializable::Header::ATTR_VERSION)->To<ui16> ();
            return object;
        }

        /// \brief
        /// Serializable insertion operator.
        /// \param[in] serializer Where to serialize the serializable.
        /// \param[in] serializable Serializable to serialize.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Serializable &serializable) {
            serializer <<
                Serializable::Header (
                    serializable.Type (),
                    serializable.Version (),
                    serializable.Size ());
            serializable.Write (serializer);
            return serializer;
        }

        /// \brief
        /// Serializable insertion operator.
        /// \param[in] node Where to serialize the serializable.
        /// \param[in] serializable Serializable to serialize.
        /// \return node.
        inline pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Serializable &serializable) {
            node <<
                Serializable::Header (
                    serializable.Type (),
                    serializable.Version (),
                    0);
            serializable.WriteXML (node);
            return node;
        }

        /// \brief
        /// Serializable insertion operator.
        /// \param[in] object Where to serialize the serializable.
        /// \param[in] serializable Serializable to serialize.
        /// \return node.
        inline JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const Serializable &serializable) {
            object <<
                Serializable::Header (
                    serializable.Type (),
                    serializable.Version (),
                    0);
            serializable.WriteJSON (object);
            return object;
        }

        /// \brief
        /// Serializable extraction operator.
        /// \param[in] serializer Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Serializable &serializable);
        /// \brief
        /// Serializable extraction operator.
        /// \param[in] node Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            const pugi::xml_node &node,
            Serializable &serializable);
        /// \brief
        /// Serializable extraction operator.
        /// \param[in] object Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            const JSON::Object &object,
            Serializable &serializable);

        /// \brief
        /// Serializable::SharedPtr extraction operator.
        /// \param[in] serializer Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
            Serializer &serializer,
            Serializable::SharedPtr &serializable);
        /// \brief
        /// Serializable::SharedPtr extraction operator.
        /// \param[in] node Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
            const pugi::xml_node &node,
            Serializable::SharedPtr &serializable);
        /// \brief
        /// Serializable::SharedPtr extraction operator.
        /// \param[in] object Serializer to extract from.
        /// \param[out] serializable Serializable to extract.
        /// \return serializer.
        _LIB_THEKOGANS_UTIL_DECL const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
            const JSON::Object &object,
            Serializable::SharedPtr &serializable);

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS(_T)
        /// Implement extraction operators for _T::SharedPtr.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS(_T)\
            inline thekogans::util::Serializer & _LIB_THEKOGANS_UTIL_API operator >> (\
                    thekogans::util::Serializer &serializer,\
                    _T::SharedPtr &serializable) {\
                serializer >> (thekogans::util::Serializable::SharedPtr &)serializable;\
                return serializer;\
            }\
            inline const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (\
                    const pugi::xml_node &node,\
                    _T::SharedPtr &serializable) {\
                node >> (thekogans::util::Serializable::SharedPtr &)serializable;\
                return node;\
            }\
            inline const thekogans::util::JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (\
                    const thekogans::util::JSON::Object &object,\
                    _T::SharedPtr &serializable) {\
                object >> (thekogans::util::Serializable::SharedPtr &)serializable;\
                return object;\
            }

        /// \struct ValueParser<Serializable::Header> Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Specialization of \see{ValueParser} for \see{Serializable::Header}.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<Serializable::Header> {
        private:
            /// \brief
            /// \see{Serializable::Header} to parse.
            Serializable::Header &value;
            /// \brief
            /// Magic value preceeding every object.
            ui32 magic;
            /// \brief
            /// Parses magic.
            ValueParser<ui32> magicParser;
            /// \brief
            /// Parses \see{Serializable::Header::type}.
            ValueParser<std::string> typeParser;
            /// \brief
            /// Parses \see{Serializable::Header::version}.
            ValueParser<ui16> versionParser;
            /// \brief
            /// Parses \see{Serializable::Header::size}.
            ValueParser<SizeT> sizeParser;
            /// \enum
            /// \see{Serializable::Header} parser is a state machine.
            /// These are it's various states.
            enum {
                /// \brief
                /// Next value is magic.
                STATE_MAGIC,
                /// \brief
                /// Next value is \see{Serializable::Header::type}.
                STATE_TYPE,
                /// \brief
                /// Next value is \see{Serializable::Header::version}.
                STATE_VERSION,
                /// \brief
                /// Next value is \see{Serializable::Header::size}.
                STATE_SIZE
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            explicit ValueParser (Serializable::Header &value_) :
                value (value_),
                magic (0),
                magicParser (magic),
                typeParser (value.type),
                versionParser (value.version),
                sizeParser (value.size),
                state (STATE_MAGIC) {}

            /// \brief
            /// Rewind the sub-parsers to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a \see{Serializable::Header} from the given serializer.
            /// \param[in] serializer Contains a complete or partial \see{Serializable::Header}.
            /// \return true == \see{Serializable::Header} was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

        /// \struct ValueParser<Serializable> Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Specialization of \see{ValueParser} for \see{Serializable}.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<Serializable> {
        protected:
            /// \brief
            /// Serializable to parse.
            Serializable &value;
            /// \brief
            /// Default serializable size;
            static const std::size_t DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024;
            /// \brief
            /// Used to twart ddos attacks. The generic 2MB might be too much.
            /// Tune this value to protect your application.
            const std::size_t maxSerializableSize;
            /// \brief
            /// Parsed serializable header.
            Serializable::Header header;
            /// \brief
            /// Parsed serializable payload.
            NetworkBuffer payload;
            /// \brief
            /// Serializable header parser.
            ValueParser<Serializable::Header> headerParser;
            /// \enum
            /// The parser is a state machine. It has two states.
            enum {
                /// \brief
                /// We're looking for a header.
                STATE_BIN_HEADER,
                /// \brief
                /// We're looting for the payload.
                STATE_SERIALIZABLE
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            /// \param[in] maxSerializableSize_ Used to protect the code against malicious actors.
            ValueParser (
                Serializable &value_,
                std::size_t maxSerializableSize_ = DEFAULT_MAX_SERIALIZABLE_SIZE) :
                value (value_),
                maxSerializableSize (maxSerializableSize_),
                headerParser (header),
                state (STATE_BIN_HEADER) {}

            /// \brief
            /// Rewind the sub-parsers to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a \see{Serializable::SharedPtr} from the given serializer.
            /// \param[in] serializer Contains a complete or partial \see{Serializable::SharedPtr}.
            /// \return true == \see{Serializable::SharedPtr} was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

        /// \struct ValueParser<Serializable::SharedPtr> Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Specialization of \see{ValueParser} for \see{Serializable::SharedPtr}.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<Serializable::SharedPtr> {
        protected:
            /// \brief
            /// Serializable to parse.
            Serializable::SharedPtr &value;
            /// \brief
            /// Default serializable size;
            static const std::size_t DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024;
            /// \brief
            /// Used to twart ddos attacks. The generic 2MB might be too much.
            /// Tune this value to protect your application.
            const std::size_t maxSerializableSize;
            /// \brief
            /// Parsed serializable header.
            Serializable::Header header;
            /// \brief
            /// Parsed serializable payload.
            NetworkBuffer payload;
            /// \brief
            /// Serializable header parser.
            ValueParser<Serializable::Header> headerParser;
            /// \enum
            /// The parser is a state machine. It has two states.
            enum {
                /// \brief
                /// We're looking for a header.
                STATE_BIN_HEADER,
                /// \brief
                /// We're looting for the payload.
                STATE_SERIALIZABLE
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            /// \param[in] maxSerializableSize_ Used to protect the code against malicious actors.
            ValueParser (
                Serializable::SharedPtr &value_,
                std::size_t maxSerializableSize_ = DEFAULT_MAX_SERIALIZABLE_SIZE) :
                value (value_),
                maxSerializableSize (maxSerializableSize_),
                headerParser (header),
                state (STATE_BIN_HEADER) {}

            /// \brief
            /// Rewind the sub-parsers to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a \see{Serializable::SharedPtr} from the given serializer.
            /// \param[in] serializer Contains a complete or partial \see{Serializable::SharedPtr}.
            /// \return true == \see{Serializable::SharedPtr} was successfully parsed,
            /// false == call back with more data.
            bool ParseValue (Serializer &serializer);
        };

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER(_T)
        /// Implement a value parser for _T::SharedPtr.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER(_T)\
            template<>\
            struct _LIB_THEKOGANS_UTIL_DECL ValueParser<_T::SharedPtr> :\
                    public thekogans::util::ValueParser<thekogans::util::Serializable::SharedPtr> {\
                ValueParser (\
                    _T::SharedPtr &value,\
                    std::size_t maxSerializableSize = DEFAULT_MAX_SERIALIZABLE_SIZE) :\
                    thekogans::util::ValueParser<thekogans::util::Serializable::SharedPtr> (\
                        (thekogans::util::Serializable::SharedPtr &)value, maxSerializableSize) {}\
            };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializable_h)
