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
#include "thekogans/util/Heap.h"
#include "thekogans/util/RefCounted.h"
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
        /// Serializable is an abstract base for all supported serializable types (See
        /// \see{Serializer}). It exposes machinery used by descendants to register
        /// themselves for dynamic discovery, creation and serializable insertion and
        /// extraction. Serializable has built in support for binary, XML and JSON
        /// serialization and de-serialization.

        struct _LIB_THEKOGANS_UTIL_DECL Serializable :
                public virtual ThreadSafeRefCounted {
            /// \brief
            /// Convenient typedef for ThreadSafeRefCounted::Ptr<Serializable>.
            typedef ThreadSafeRefCounted::Ptr<Serializable> Ptr;

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
                    const char *type_,
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
                    const char *type_,
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

            /// \brief
            /// typedef for the Serializable binary factory function.
            typedef Ptr (*BinFactory) (
                const BinHeader & /*header*/,
                Serializer & /*serializer*/);
            /// \brief
            /// typedef for the Serializable XML factory function.
            typedef Ptr (*XMLFactory) (
                const TextHeader & /*header*/,
                const pugi::xml_node & /*node*/);
            /// \brief
            /// typedef for the Serializable JSON factory function.
            typedef Ptr (*JSONFactory) (
                const TextHeader & /*header*/,
                const JSON::Object & /*object*/);
            /// \brief
            /// typedef for Serializable factories.
            typedef std::tuple<BinFactory, XMLFactory, JSONFactory> Factories;
            /// \brief
            /// typedef for the Serializable map.
            typedef std::map<std::string, Factories> Map;
            /// \brief
            /// Controls Map's lifetime.
            /// \return Serializable map.
            static Map &GetMap ();
            /// \struct Serializable::MapInitializer Serializable.h thekogans/util/Serializable.h
            ///
            /// \brief
            /// MapInitializer is used to initialize the Serializable::map.
            /// It should not be used directly, and instead is included
            /// in THEKOGANS_UTIL_DECLARE_SERIALIZABLE/THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE.
            /// If you are deriving a serializable from Serializable, and you want
            /// it to be dynamically discoverable/creatable, add
            /// THEKOGANS_UTIL_DECLARE_SERIALIZABLE to it's declaration,
            /// and THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE to it's definition.
            struct _LIB_THEKOGANS_UTIL_DECL MapInitializer {
                /// \brief
                /// ctor. Add serializable of type, and factory for creating it
                /// to the Serializable::map
                /// \param[in] type Serializable type (it's class name).
                /// \param[in] factories Serializable creation factories.
                MapInitializer (
                    const std::string &type,
                    Factories factory);
            };

            /// \brief
            /// dtor.
            virtual ~Serializable () {}

            /// \brief
            /// Check the map for the given type.
            /// \return true == The given type is in the map.
            static bool ValidateType (const std::string &type);

            /// \brief
            /// Return the size of the serializable including the header.
            /// Use of this API is mendatory as virtual std::size_t Size ()
            /// (below) is protected.
            /// \return Size of the serializable including the header.
            static std::size_t Size (const Serializable &serializable);

            /// \brief
            /// Return Serializable type.
            /// \return Serializable type.
            inline std::string GetType () const {
                return Type ();
            }

        protected:
            /// \brief
            /// Return serializable type (it's class name).
            /// \return Serializable type.
            virtual const char *Type () const = 0;

            /// \brief
            /// Return the serializable version.
            /// \return Serializable version.
            virtual ui16 Version () const = 0;

            /// \brief
            /// Return the serializable size (not including the header).
            /// \return Serializable size.
            virtual std::size_t Size () const = 0;

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

            /// \brief
            /// Needs access to BinHeader.
            friend Serializer &operator << (
                Serializer &serializer,
                const BinHeader &header);
            /// \brief
            /// Needs access to BinHeader.
            friend Serializer &operator >> (
                Serializer &serializer,
                BinHeader &header);
            /// \brief
            /// Needs access to TextHeader.
            friend pugi::xml_node &operator << (
                pugi::xml_node &node,
                const TextHeader &header);
            /// \brief
            /// Needs access to TextHeader.
            friend const pugi::xml_node &operator >> (
                const pugi::xml_node &node,
                TextHeader &header);
            /// \brief
            /// Needs access to TextHeader and Write.
            friend Serializer &operator << (
                Serializer &serializer,
                const Serializable &serializable);
            /// \brief
            /// Needs access to TextHeader and Write.
            friend pugi::xml_node &operator << (
                pugi::xml_node &node,
                const Serializable &serializable);
            /// \brief
            /// Needs access to TextHeader and Write.
            friend JSON::Object &operator << (
                JSON::Object &object,
                const Serializable &serializable);
        };

        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON(type, lock)
        /// Common code used by Static and Shared versions THEKOGANS_UTIL_DECLARE_SERIALIZABLE.
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON(type, lock)\
        public:\
            typedef thekogans::util::ThreadSafeRefCounted::Ptr<type> Ptr;\
        protected:\
            THEKOGANS_UTIL_DECLARE_HEAP_WITH_LOCK (type, lock)\
            type (\
                    const thekogans::util::Serializable::BinHeader &header,\
                    thekogans::util::Serializer &serializer) {\
                Read (header, serializer);\
            }\
            type (\
                    const thekogans::util::Serializable::TextHeader &header,\
                    const pugi::xml_node &node) {\
                Read (header, node);\
            }\
            type (\
                    const thekogans::util::Serializable::TextHeader &header,\
                    const thekogans::util::JSON::Object &object) {\
                Read (header, object);\
            }\
            static thekogans::util::Serializable::Ptr BinCreate (\
                    const thekogans::util::Serializable::BinHeader &header,\
                    thekogans::util::Serializer &serializer) {\
                return thekogans::util::Serializable::Ptr (\
                    new type (header, serializer));\
            }\
            static thekogans::util::Serializable::Ptr XMLCreate (\
                    const thekogans::util::Serializable::TextHeader &header,\
                    const pugi::xml_node &node) {\
                return thekogans::util::Serializable::Ptr (\
                    new type (header, node));\
            }\
            static thekogans::util::Serializable::Ptr JSONCreate (\
                    const thekogans::util::Serializable::TextHeader &header,\
                    const thekogans::util::JSON::Object &object) {\
                return thekogans::util::Serializable::Ptr (\
                    new type (header, object));\
            }\
            friend thekogans::util::Serializer &operator >> (\
                thekogans::util::Serializer &serializer,\
                type &serializable);\
            friend const pugi::xml_node &operator >> (\
                const pugi::xml_node &node,\
                type &serializable);\
            friend const thekogans::util::JSON::Object &operator >> (\
                const thekogans::util::JSON::Object &object,\
                type &serializable);\
            template<typename>\
            friend struct thekogans::util::ValueParser;\
        public:\
            static const char *TYPE;\
            static const thekogans::util::ui16 VERSION;\
        protected:\
            virtual const char *Type () const {\
                return TYPE;\
            }\
            virtual thekogans::util::ui16 Version () const {\
                return VERSION;\
            }\
        public:

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON(
        ///     type, version, lock, minSerializablesInPage, allocator)
        /// Common code used by Static and Shared versions THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON(\
                type, version, lock, minSerializablesInPage, allocator)\
            const char *type::TYPE = #type;\
            const thekogans::util::ui16 type::VERSION = version;\
            THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK_EX_AND_ALLOCATOR (\
                type,\
                lock,\
                minSerializablesInPage,\
                allocator)

    #if defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SymmetricKey : public util::Serializable {
        ///     THEKOGANS_CRYPTO_DECLARE_SERIALIZABLE (SymmetricKey)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON (type, lock)\
            static void StaticInit ();

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(type, version, lock, minSerializablesInPage, allocator)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// #if !defined (THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///     #define THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE 16
        /// #endif // !defined (THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///
        /// THEKOGANS_CRYPTO_IMPLEMENT_SERIALIZABLE (
        ///     SymmetricKey,
        ///     1,
        ///     THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(\
                type, version, lock, minSerializablesInPage, allocator)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON (\
                type, version, lock, minSerializablesInPage, allocator)\
            void type::StaticInit () {\
                static volatile bool registered = false;\
                static thekogans::util::SpinLock spinLock;\
                thekogans::util::LockGuard<thekogans::util::SpinLock> guard (spinLock);\
                if (!registered) {\
                    std::pair<Map::iterator, bool> result =\
                        GetMap ().insert (\
                            Map::value_type (\
                                #type,\
                                thekogans::util::Serializable::Factories (\
                                    type::BinCreate,\
                                    type::XMLCreate,\
                                    type::JSONCreate)));\
                    if (!result.second) {\
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                            "'%s' is already registered.", #type);\
                    }\
                    registered = true;\
                }\
            }
    #else // defined (THEKOGANS_UTIL_TYPE_Static)
        /// \def THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)
        /// Dynamic discovery macro. Add this to your class declaration.
        /// Example:
        /// \code{.cpp}
        /// struct _LIB_THEKOGANS_UTIL_DECL SymmetricKey : public util::Serializable {
        ///     THEKOGANS_CRYPTO_DECLARE_SERIALIZABLE (SymmetricKey)
        ///     ...
        /// };
        /// \endcode
        #define THEKOGANS_UTIL_DECLARE_SERIALIZABLE(type, lock)\
            THEKOGANS_UTIL_DECLARE_SERIALIZABLE_COMMON (type, lock)\
            static const thekogans::util::Serializable::MapInitializer mapInitializer;

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(
        ///     type, version, lock, minSerializablesInPage, allocator)
        /// Dynamic discovery macro. Instantiate one of these in the class cpp file.
        /// Example:
        /// \code{.cpp}
        /// #if !defined (THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///     #define THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE 16
        /// #endif // !defined (THEKOGANS_CRYPTO_MIN_SYMMETRIC_KEYS_IN_PAGE)
        ///
        /// THEKOGANS_CRYPTO_IMPLEMENT_SERIALIZABLE (
        ///     SymmetricKey,
        ///     1,
        ///     THEKOGANS_CRYPTO_MIN_SYMMETRIC_SERIALIZABLES_IN_PAGE)
        /// \endcode
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE(\
                type, version, lock, minSerializablesInPage, allocator)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_COMMON (\
                type, version, lock, minSerializablesInPage, allocator)\
            const thekogans::util::Serializable::MapInitializer type::mapInitializer (\
                #type,\
                thekogans::util::Serializable::Factories (\
                    type::BinCreate,\
                    type::XMLCreate,\
                    type::JSONCreate));
    #endif // defined (THEKOGANS_UTIL_TYPE_Static)

        /// \brief
        /// Serializable::BinHeader insertion operator.
        /// \param[in] serializer Where to serialize the serializable header.
        /// \param[in] header Serializable::BinHeader to serialize.
        /// \return serializer.
        inline Serializer &operator << (
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
        inline Serializer &operator >> (
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
        inline pugi::xml_node &operator << (
                pugi::xml_node &node,
                const Serializable::TextHeader &header) {
            node.append_attribute (Serializable::TextHeader::ATTR_TYPE).set_value (header.type.c_str ());
            node.append_attribute (Serializable::TextHeader::ATTR_VERSION).set_value (ui32Tostring (header.version).c_str ());
            return node;
        }

        /// \brief
        /// Serializable::TextHeader extraction operator.
        /// \param[in] serializer Where to deserialize the serializable header.
        /// \param[in] header Serializable::TextHeader to deserialize.
        /// \return node.
        inline const pugi::xml_node &operator >> (
                const pugi::xml_node &node,
                Serializable::TextHeader &header) {
            header.type = node.attribute (Serializable::TextHeader::ATTR_TYPE).value ();
            header.version = stringToui16 (node.attribute (Serializable::TextHeader::ATTR_VERSION).value ());
            return node;
        }

        /// \brief
        /// Serializable::TextHeader insertion operator.
        /// \param[in] object Where to serialize the serializable header.
        /// \param[in] header Serializable::TextHeader to serialize.
        /// \return node.
        inline JSON::Object &operator << (
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
        inline const JSON::Object &operator >> (
                const JSON::Object &object,
                Serializable::TextHeader &header) {
            header.type = object.Get<JSON::String> (Serializable::TextHeader::ATTR_TYPE)->value;
            header.version = object.Get<JSON::Number> (Serializable::TextHeader::ATTR_VERSION)->To<ui16> ();
            return object;
        }

        /// \brief
        /// Serializable insertion operator.
        /// \param[in] serializer Where to serialize the serializable.
        /// \param[in] serializable Serializable to serialize.
        /// \return serializer.
        inline Serializer &operator << (
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
        inline pugi::xml_node &operator << (
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
        inline JSON::Object &operator << (
                JSON::Object &object,
                const Serializable &serializable) {
            object <<
                Serializable::TextHeader (
                    serializable.Type (),
                    serializable.Version ());
            serializable.Write (object);
            return object;
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

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_EXTRACTION_OPERATORS(_T)
        /// Implement \see{Serializable} extraction operators.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_EXTRACTION_OPERATORS(_T)\
            inline thekogans::util::Serializer &operator >> (\
                    thekogans::util::Serializer &serializer,\
                    _T &serializable) {\
                thekogans::util::Serializable::BinHeader header;\
                serializer >> header;\
                if (header.magic == thekogans::util::MAGIC32 &&\
                        header.type == serializable.GetType ()) {\
                    serializable.Read (header, serializer);\
                    return serializer;\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "Corrupt serializable header. Got %s, expecting %s.",\
                        header.type.c_str (),\
                        serializable.GetType ().c_str ());\
                }\
            }\
            inline const pugi::xml_node &operator >> (\
                    const pugi::xml_node &node,\
                    _T &serializable) {\
                thekogans::util::Serializable::TextHeader header;\
                node >> header;\
                if (header.type == serializable.GetType ()) {\
                    serializable.Read (header, node);\
                    return node;\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "Corrupt serializable header. Got %s, expecting %s.",\
                        header.type.c_str (),\
                        serializable.GetType ().c_str ());\
                }\
            }\
            inline const thekogans::util::JSON::Object &operator >> (\
                    const thekogans::util::JSON::Object &object,\
                    _T &serializable) {\
                thekogans::util::Serializable::TextHeader header;\
                object >> header;\
                if (header.type == serializable.GetType ()) {\
                    serializable.Read (header, object);\
                    return object;\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "Corrupt serializable header. Got %s, expecting %s.",\
                        header.type.c_str (),\
                        serializable.GetType ().c_str ());\
                }\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_EXTRACTION_OPERATORS(_T)
        /// Implement \see{Serializable::Ptr} extraction operators.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_EXTRACTION_OPERATORS(_T)\
            inline thekogans::util::Serializer &operator >> (\
                    thekogans::util::Serializer &serializer,\
                    _T::Ptr &serializable) {\
                thekogans::util::Serializable::BinHeader header;\
                serializer >> header;\
                if (header.magic == thekogans::util::MAGIC32) {\
                    thekogans::util::Serializable::Map::iterator it =\
                        thekogans::util::Serializable::GetMap ().find (header.type);\
                    if (it != thekogans::util::Serializable::GetMap ().end ()) {\
                        serializable =\
                            thekogans::util::dynamic_refcounted_pointer_cast<_T> (\
                                std::get<0> (it->second) (header, serializer));\
                        return serializer;\
                    }\
                    else {\
                        THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                            "No registered factory for serializable '%s'.",\
                            header.type.c_str ());\
                    }\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "Corrupt serializable '%s' header.",\
                        header.type.c_str ());\
                }\
            }\
            inline const pugi::xml_node &operator >> (\
                    const pugi::xml_node &node,\
                    _T::Ptr &serializable) {\
                thekogans::util::Serializable::TextHeader header;\
                node >> header;\
                thekogans::util::Serializable::Map::iterator it =\
                    thekogans::util::Serializable::GetMap ().find (header.type);\
                if (it != thekogans::util::Serializable::GetMap ().end ()) {\
                    serializable =\
                        thekogans::util::dynamic_refcounted_pointer_cast<_T> (\
                            std::get<1> (it->second) (header, node)); \
                    return node;\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "No registered factory for serializable '%s'.",\
                        header.type.c_str ());\
                }\
            }\
            inline const thekogans::util::JSON::Object &operator >> (\
                    const thekogans::util::JSON::Object &object,\
                    _T::Ptr &serializable) {\
                thekogans::util::Serializable::TextHeader header;\
                object >> header;\
                thekogans::util::Serializable::Map::iterator it =\
                    thekogans::util::Serializable::GetMap ().find (header.type);\
                if (it != thekogans::util::Serializable::GetMap ().end ()) {\
                    serializable =\
                        thekogans::util::dynamic_refcounted_pointer_cast<_T> (\
                            std::get<2> (it->second) (header, object)); \
                    return object;\
                }\
                else {\
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                        "No registered factory for serializable '%s'.",\
                        header.type.c_str ());\
                }\
            }

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS(_T)
        /// Implement \see{Serializable} extraction operators.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_EXTRACTION_OPERATORS(_T)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_EXTRACTION_OPERATORS(_T)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_EXTRACTION_OPERATORS(_T)

        /// \brief
        /// Implement \see{Serializable} extraction operators.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_EXTRACTION_OPERATORS (Serializable)

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_VALUE_PARSER(_T)
        /// Implement \see{Serializable} \see{ValueParser}.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_VALUE_PARSER(_T)\
            template<>\
            struct thekogans::util::ValueParser<_T> {\
            private:\
                _T &value;\
                enum {\
                    DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024\
                };\
                const std::size_t maxSerializableSize;\
                thekogans::util::Serializable::BinHeader header;\
                thekogans::util::Buffer payload;\
                thekogans::util::ValueParser<thekogans::util::Serializable::BinHeader> headerParser;\
                enum {\
                    STATE_BIN_HEADER,\
                    STATE_SERIALIZABLE\
                } state;\
            public:\
                ValueParser (\
                    _T &value_,\
                    std::size_t maxSerializableSize_ = DEFAULT_MAX_SERIALIZABLE_SIZE) :\
                    value (value_),\
                    maxSerializableSize (maxSerializableSize_),\
                    payload (NetworkEndian),\
                    headerParser (header),\
                    state (STATE_BIN_HEADER) {}\
                inline void Reset () {\
                    payload.Resize (0);\
                    headerParser.Reset ();\
                    state = STATE_BIN_HEADER;\
                }\
                inline bool ParseValue (thekogans::util::Serializer &serializer) {\
                    if (state == STATE_BIN_HEADER) {\
                        if (headerParser.ParseValue (serializer)) {\
                            if (header.size > 0 && header.size <= maxSerializableSize) {\
                                THEKOGANS_UTIL_TRY {\
                                    payload.Resize (header.size);\
                                    state = STATE_SERIALIZABLE;\
                                }\
                                THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                                    Reset ();\
                                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                                }\
                            }\
                            else {\
                                Reset ();\
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                                    "Invalid serializable length: %u.",\
                                    header.size);\
                            }\
                        }\
                    }\
                    if (state == STATE_SERIALIZABLE) {\
                        payload.AdvanceWriteOffset (\
                            serializer.Read (\
                                payload.GetWritePtr (),\
                                payload.GetDataAvailableForWriting ()));\
                        if (payload.IsFull ()) {\
                            value.Read (header, payload);\
                            Reset ();\
                            return true;\
                        }\
                    }\
                    return false;\
                }\
            };

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_VALUE_PARSER(_T)
        /// Implement \see{Serializable::Ptr} \see{ValueParser}.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_VALUE_PARSER(_T)\
            template<>\
            struct thekogans::util::ValueParser<_T::Ptr> {\
            private:\
                _T::Ptr &value;\
                enum {\
                    DEFAULT_MAX_SERIALIZABLE_SIZE = 2 * 1024 * 1024\
                };\
                const std::size_t maxSerializableSize;\
                thekogans::util::Serializable::BinHeader header;\
                thekogans::util::Buffer payload;\
                thekogans::util::ValueParser<thekogans::util::Serializable::BinHeader> headerParser;\
                enum {\
                    STATE_BIN_HEADER,\
                    STATE_SERIALIZABLE\
                } state;\
            public:\
                ValueParser (\
                    _T::Ptr &value_,\
                    std::size_t maxSerializableSize_ = DEFAULT_MAX_SERIALIZABLE_SIZE) :\
                    value (value_),\
                    maxSerializableSize (maxSerializableSize_),\
                    payload (NetworkEndian),\
                    headerParser (header),\
                    state (STATE_BIN_HEADER) {}\
                inline void Reset () {\
                    payload.Resize (0);\
                    headerParser.Reset ();\
                    state = STATE_BIN_HEADER;\
                }\
                inline bool ParseValue (thekogans::util::Serializer &serializer) {\
                    if (state == STATE_BIN_HEADER) {\
                        if (headerParser.ParseValue (serializer)) {\
                            if (header.size > 0 && header.size <= maxSerializableSize) {\
                                THEKOGANS_UTIL_TRY {\
                                    payload.Resize (header.size);\
                                    state = STATE_SERIALIZABLE;\
                                }\
                                THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                                    Reset ();\
                                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception);\
                                }\
                            }\
                            else {\
                                Reset ();\
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                                    "Invalid serializable length: %u.",\
                                    header.size);\
                            }\
                        }\
                    }\
                    if (state == STATE_SERIALIZABLE) {\
                        payload.AdvanceWriteOffset (\
                            serializer.Read (\
                                payload.GetWritePtr (),\
                                payload.GetDataAvailableForWriting ()));\
                        if (payload.IsFull ()) {\
                            thekogans::util::Serializable::Map::iterator it =\
                                thekogans::util::Serializable::GetMap ().find (header.type);\
                            if (it != thekogans::util::Serializable::GetMap ().end ()) {\
                                THEKOGANS_UTIL_TRY {\
                                    value =\
                                        thekogans::util::dynamic_refcounted_pointer_cast<_T> (\
                                            std::get<0> (it->second) (header, payload));\
                                    Reset ();\
                                    return true;\
                                }\
                                THEKOGANS_UTIL_CATCH (thekogans::util::Exception) {\
                                    Reset ();\
                                    THEKOGANS_UTIL_RETHROW_EXCEPTION (exception); \
                                }\
                            }\
                            else {\
                                Reset ();\
                                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (\
                                    "No registered factory for serializable '%s'.",\
                                    header.type.c_str ());\
                            }\
                        }\
                    }\
                    return false;\
                }\
            };

        /// \def THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER(_T)
        /// Implement \see{Serializable} \see{ValueParser}.
        #define THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_VALUE_PARSER(_T)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_REF_VALUE_PARSER(_T)\
            THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_VALUE_PARSER(_T)

        /// \brief
        /// Implement \see{Serializable} \see{ValueParser}.
        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE_PTR_VALUE_PARSER (Serializable)

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Serializable_h)
