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
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_DYNAMIC_CREATABLE_BASE (Serializable)

            /// \struct Serializable::BinHeader Serializable.h thekogans/util/Serializable.h
            ///
            /// \brief
            /// Binary header containing enough info to deserialize the serializable instance.
            struct _LIB_THEKOGANS_UTIL_DECL BinHeader {
                /// \brief
                /// MAGIC32
                ui32 magic;
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
                BinHeader () :
                    magic (0),
                    version (0),
                    size (0) {}
                /// \brief
                /// ctor.
                /// \param[in] type_ Serializable type (it's class name).
                /// \param[in] version_ Serializable version.
                /// \param[in] size_ Serializable size in bytes (not including the header).
                BinHeader (
                    const std::string &type_,
                    ui16 version_,
                    std::size_t size_) :
                    magic (MAGIC32),
                    type (type_),
                    version (version_),
                    size (size_) {}

                /// \brief
                /// Return the header size.
                /// \return BinHeader size.
                inline std::size_t Size () const {
                    return
                        Serializer::Size (magic) +
                        Serializer::Size (type) +
                        Serializer::Size (version) +
                        Serializer::Size (size);
                }

                /// \brief
                /// "BinHeader"
                static const char * const TAG_BIN_HEADER;
                /// \brief
                /// "Magic"
                static const char * const ATTR_MAGIC;
                /// \brief
                /// "Type"
                static const char * const ATTR_TYPE;
                /// \brief
                /// "Version"
                static const char * const ATTR_VERSION;
                /// \brief
                /// "Size"
                static const char * const ATTR_SIZE;

                /// \brief
                /// Parse the header from an xml dom that looks like this;
                /// <Header Magic = "FARS"
                ///         Type = ""
                ///         Version = ""
                ///         Size = ""
                ///         ...>
                /// \param[in] node DOM representation of a header.
                void Parse (const pugi::xml_node &node);
                /// \brief
                /// Return the XML representation of a header.
                /// \param[in] indentationLevel How far to indent the leading tag.
                /// \param[in] tagName The name of the leading tag.
                /// \return XML representation of a header.
                std::string ToString (
                    std::size_t indentationLevel = 0,
                    const char *tagName = TAG_BIN_HEADER) const;
            };

            /// \struct Serializable::TextHeader Serializable.h thekogans/util/Serializable.h
            ///
            /// \brief
            /// TextHeader containing enough info to deserialize the serializable instance.
            struct _LIB_THEKOGANS_UTIL_DECL TextHeader {
                /// \brief
                /// Serializable type (it's class name).
                std::string type;
                /// \brief
                /// Serializable version.
                ui16 version;

                /// \brief
                /// ctor.
                TextHeader () :
                    version (0) {}
                /// \brief
                /// ctor.
                /// \param[in] type_ Serializable type (it's class name).
                /// \param[in] version_ Serializable version.
                TextHeader (
                    const std::string &type_,
                    ui16 version_) :
                    type (type_),
                    version (version_) {}

                /// \brief
                /// "Type"
                static const char * const ATTR_TYPE;
                /// \brief
                /// "Version"
                static const char * const ATTR_VERSION;

                /// \brief
                /// Read header attributes from an xml dom.
                /// \param[in] node Node containing header attributes.
                void Read (const pugi::xml_node &node);
                /// \brief
                /// Write a header to the xml dom.
                /// \param[out] node Node that will recieve the header attributes.
                void Write (pugi::xml_node &node) const;
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
                const BinHeader & /*header*/,
                Serializer & /*serializer*/) = 0;
            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Write (Serializer & /*serializer*/) const = 0;

            /// \brief
            /// Read a Serializable from an XML DOM.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void Read (
                const TextHeader & /*header*/,
                const pugi::xml_node & /*node*/) = 0;
            /// \brief
            /// Write a Serializable to the XML DOM.
            /// \param[out] node Parent node.
            virtual void Write (pugi::xml_node & /*node*/) const = 0;

            /// \brief
            /// Read a Serializable from an JSON DOM.
            /// \param[in] node JSON DOM representation of a Serializable.
            virtual void Read (
                const TextHeader & /*header*/,
                const JSON::Object & /*object*/) = 0;
            /// \brief
            /// Write a Serializable to the JSON DOM.
            /// \param[out] node Parent node.
            virtual void Write (JSON::Object & /*object*/) const = 0;
        };

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_OVERRIDE(_T)
        /// Common defines for Serializable.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_OVERRIDE(_T)\
        public:\
            static const thekogans::util::ui16 VERSION;\
            virtual thekogans::util::ui16 Version () const noexcept override;

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(_T, version)
        /// Serializable overrides.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE(_T, version)\
            const thekogans::util::ui16 _T::VERSION = version;\
            thekogans::util::ui16 _T::Version () const noexcept {\
                return VERSION;\
            }

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
                thekogans::util::Serializable::TYPE, __VA_ARGS__)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_OVERRIDE (_T, version)

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
            /// \brief
            /// Underlying blob type.
            std::string type;
            /// \brief
            /// Underlying blob version.
            ui16 version;
            /// \brief
            /// Underlying blob size in bytes (not including the header).
            std::size_t size;
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
            /// ctor.
            Blob () :
                version (0),
                size (0) {}

            /// \brief
            /// Return DynamicCreatable type (it's class name).
            /// \return DynamicCreatable type (it's class name).
            virtual const char *Type () const noexcept override;

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
            /// \param[in] header Serializable::BinHeader to deserialize.
            /// \param[in] serializer Serializer to read the serializable from.
            virtual void Read (
                const BinHeader &header,
                Serializer &serializer) override;
            /// \brief
            /// Write the serializable to the given serializer.
            /// \param[out] serializer Serializer to write the serializable to.
            virtual void Write (Serializer &serializer) const override;

            /// \brief
            /// Read a Serializable from an XML DOM.
            /// \param[in] header Serializable::TextHeader to deserialize.
            /// \param[in] node XML DOM representation of a Serializable.
            virtual void Read (
                const TextHeader &header,
                const pugi::xml_node &node_) override;
            /// \brief
            /// Write a Serializable to the XML DOM.
            /// \param[out] node_ Parent node.
            virtual void Write (pugi::xml_node &node_) const override;

            /// \brief
            /// Read a Serializable from an JSON DOM.
            /// \param[in] header Serializable::Texteader to deserialize.
            /// \param[in] object_ JSON DOM representation of a Serializable.
            virtual void Read (
                const TextHeader &header,
                const JSON::Object &object_) override;
            /// \brief
            /// Write a Serializable to the JSON DOM.
            /// \param[out] object_ Parent node.
            virtual void Write (JSON::Object &object_) const override;
        };

        /// \brief
        /// Serializable::BinHeader insertion operator.
        /// \param[in] serializer Where to serialize the serializable header.
        /// \param[in] header Serializable::BinHeader to serialize.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator << (
                Serializer &serializer,
                const Serializable::BinHeader &header) {
            serializer <<
                header.magic <<
                header.type <<
                header.version <<
                header.size;
            return serializer;
        }

        /// \brief
        /// Serializable::BinHeader extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::BinHeader to deserialize.
        /// \return serializer.
        inline Serializer & _LIB_THEKOGANS_UTIL_API operator >> (
                Serializer &serializer,
                Serializable::BinHeader &header) {
            serializer >>
                header.magic >>
                header.type >>
                header.version >>
                header.size;
            return serializer;
        }

        /// \brief
        /// Serializable::TextHeader insertion operator.
        /// \param[in] node Where to serialize the serializable header.
        /// \param[in] header Serializable::TextHeader to serialize.
        /// \return node.
        inline pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator << (
                pugi::xml_node &node,
                const Serializable::TextHeader &header) {
            node.append_attribute (
                Serializable::TextHeader::ATTR_TYPE).set_value (header.type.c_str ());
            node.append_attribute (
                Serializable::TextHeader::ATTR_VERSION).set_value (
                    ui32Tostring (header.version).c_str ());
            return node;
        }

        /// \brief
        /// Serializable::TextHeader extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::TextHeader to deserialize.
        /// \return node.
        inline const pugi::xml_node & _LIB_THEKOGANS_UTIL_API operator >> (
                const pugi::xml_node &node,
                Serializable::TextHeader &header) {
            header.type = node.attribute (Serializable::TextHeader::ATTR_TYPE).value ();
            header.version = stringToui16 (
                node.attribute (Serializable::TextHeader::ATTR_VERSION).value ());
            return node;
        }

        /// \brief
        /// Serializable::TextHeader insertion operator.
        /// \param[in] object Where to serialize the serializable header.
        /// \param[in] header Serializable::TextHeader to serialize.
        /// \return node.
        inline JSON::Object & _LIB_THEKOGANS_UTIL_API operator << (
                JSON::Object &object,
                const Serializable::TextHeader &header) {
            object.Add<const std::string &> (
                Serializable::TextHeader::ATTR_TYPE,
                header.type);
            object.Add (
                Serializable::TextHeader::ATTR_VERSION,
                header.version);
            return object;
        }

        /// \brief
        /// Serializable::TextHeader extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::TextHeader to deserialize.
        /// \return node.
        inline const JSON::Object & _LIB_THEKOGANS_UTIL_API operator >> (
                const JSON::Object &object,
                Serializable::TextHeader &header) {
            header.type = object.Get<JSON::String> (
                Serializable::TextHeader::ATTR_TYPE)->value;
            header.version = object.Get<JSON::Number> (
                Serializable::TextHeader::ATTR_VERSION)->To<ui16> ();
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
                Serializable::BinHeader (
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
                Serializable::TextHeader (
                    serializable.Type (),
                    serializable.Version ());
            serializable.Write (node);
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
                Serializable::TextHeader (
                    serializable.Type (),
                    serializable.Version ());
            serializable.Write (object);
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

        /// \struct ValueParser<Serializable::BinHeader> Serializable.h thekogans/util/Serializable.h
        ///
        /// \brief
        /// Specialization of \see{ValueParser} for \see{Serializable::BinHeader}.

        template<>
        struct _LIB_THEKOGANS_UTIL_DECL ValueParser<Serializable::BinHeader> {
        private:
            /// \brief
            /// \see{Serializable::BinHeader} to parse.
            Serializable::BinHeader &value;
            /// \brief
            /// Parses \see{Serializable::BinHeader::magic}.
            ValueParser<ui32> magicParser;
            /// \brief
            /// Parses \see{Serializable::BinHeader::type}.
            ValueParser<std::string> typeParser;
            /// \brief
            /// Parses \see{Serializable::BinHeader::version}.
            ValueParser<ui16> versionParser;
            /// \brief
            /// Parses \see{Serializable::BinHeader::size}.
            ValueParser<SizeT> sizeParser;
            /// \enum
            /// \see{Serializable::BinHeader} parser is a state machine.
            /// These are it's various states.
            enum {
                /// \brief
                /// Next value is \see{Serializable::BinHeader::magic}.
                STATE_MAGIC,
                /// \brief
                /// Next value is \see{Serializable::BinHeader::type}.
                STATE_TYPE,
                /// \brief
                /// Next value is \see{Serializable::BinHeader::version}.
                STATE_VERSION,
                /// \brief
                /// Next value is \see{Serializable::BinHeader::size}.
                STATE_SIZE
            } state;

        public:
            /// \brief
            /// ctor.
            /// \param[out] value_ Value to parse.
            explicit ValueParser (Serializable::BinHeader &value_) :
                value (value_),
                magicParser (value.magic),
                typeParser (value.type),
                versionParser (value.version),
                sizeParser (value.size),
                state (STATE_MAGIC) {}

            /// \brief
            /// Rewind the sub-parsers to get them ready for the next value.
            void Reset ();

            /// \brief
            /// Try to parse a \see{Serializable::BinHeader} from the given serializer.
            /// \param[in] serializer Contains a complete or partial \see{Serializable::BinHeader}.
            /// \return true == \see{Serializable::BinHeader} was successfully parsed,
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
            enum {
                DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024
            };
            /// \brief
            /// Used to twart ddos attacks. The generic 2MB might be too much.
            /// Tune this value to protect your application.
            const std::size_t maxSerializableSize;
            /// \brief
            /// Parsed serializable header.
            Serializable::BinHeader header;
            /// \brief
            /// Parsed serializable payload.
            NetworkBuffer payload;
            /// \brief
            /// Serializable header parser.
            ValueParser<Serializable::BinHeader> headerParser;
            /// \brief
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
            enum {
                DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024
            };
            /// \brief
            /// Used to twart ddos attacks. The generic 2MB might be too much.
            /// Tune this value to protect your application.
            const std::size_t maxSerializableSize;
            /// \brief
            /// Parsed serializable header.
            Serializable::BinHeader header;
            /// \brief
            /// Parsed serializable payload.
            NetworkBuffer payload;
            /// \brief
            /// Serializable header parser.
            ValueParser<Serializable::BinHeader> headerParser;
            /// \brief
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
