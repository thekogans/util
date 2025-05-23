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

#if !defined (__thekogans_util_IntrusiveList_h)
#define __thekogans_util_IntrusiveList_h

#include <cstddef>
#include <cassert>
#include <utility>
#include <functional>
#include "thekogans/util/Types.h"

namespace thekogans {
    namespace util {

        /// \struct IntrusiveList IntrusiveList.h thekogans/util/IntrusiveList.h
        ///
        /// \brief
        /// A simple intrusive list template. Very useful when std::list overhead
        /// (memory management) is not appropriate. The list is designed such that
        /// nodes derived from IntrusiveList::Node can reside in more than one list
        /// at a time (through multiple inheritance). IntrusiveList eschews all
        /// ownership semantics.
        ///
        /// Here is a canonical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct bar;
        ///
        /// enum {
        ///     LIST1_ID = 1,
        ///     LIST2_ID = 2
        /// };
        ///
        /// using List1 = util::IntrusiveList<bar, LIST1_ID>;
        /// using List2 = util::IntrusiveList<bar, LIST2_ID>;
        ///
        /// struct bar :
        ///         public List1::Node,
        ///         public List2::Node {
        ///     ...
        /// };
        ///
        /// void foo () {
        ///     bar b;
        ///
        ///     List1 list1;
        ///     list1.push_back (&b);
        ///
        ///     List2 list2;
        ///     list2.push_back (&b);
        /// }
        /// \endcode
        ///
        /// NOTE: Try as I might, I cannot make IntrusiveList work with partially
        /// defined types. Therefore, your design will have to take in to account
        /// the fact that all IntrusiveList nodes will have to be fully defined
        /// before defining the lists that contain them.
        ///
        /// IMPORTANT: In keeping with similar design principles as are used for
        /// other 'basic' types, IntrusiveList is not thread safe.
        ///
        /// VERY IMPORTANT: Because of the design of IntrusiveList you cannot
        /// store the same node twice in the list. Use IntrusiveList::contains
        /// to tell if the given node is already in the list. This API is used
        /// by all insertion APIs to check containment before inserting. This
        /// is why they return a bool (to let the caller know whether the node
        /// has been inserted).

        template<
            typename T,
            i32 ID = 0>
        struct IntrusiveList {
            /// \struct IntrusiveList::Node IntrusiveList.h thekogans/util/IntrusiveList.h
            ///
            /// \brief
            /// For every IntrusiveList an object will reside in,
            /// it must derive from that IntrusiveList::Node.
            struct Node {
                /// \brief
                /// Pointer to previous node.
                T *prev;
                /// \brief
                /// Pointer to next node.
                T *next;
                /// \brief
                /// true if the node is in the list.
                bool inList;

                /// \brief
                /// ctor.
                Node () :
                    prev (0),
                    next (0),
                    inList (false) {}
                /// \brief
                /// dtor.
                virtual ~Node () {}
            };
            /// \brief
            /// Pointer to the head of the list.
            T *head;
            /// \brief
            /// Pointer to the tail of the list.
            T *tail;
            /// \brief
            /// Count of nodes in the list.
            std::size_t count;

            /// \brief
            /// ctor.
            IntrusiveList () :
                head (0),
                tail (0),
                count (0) {}
            /// \brief
            /// Move ctor.
            /// \param[in,out] other IntrusiveList to move.
            IntrusiveList (IntrusiveList<T, ID> &&other) :
                    head (0),
                    tail (0),
                    count (0) {
                swap (other);
            }
            /// \brief
            /// dtor.
            ~IntrusiveList () {
                clear ();
            }

            /// \brief
            /// Move assignment operator.
            /// \param[in,out] other IntrusiveList to move.
            /// \return *this.
            inline IntrusiveList &operator = (IntrusiveList<T, ID> &&other) {
                if (this != &other) {
                    IntrusiveList<T, ID> temp (std::move (other));
                    swap (temp);
                }
                return *this;
            }

            /// \brief
            /// Concatenate the given list to the tail of this one.
            /// \param[in,out] other IntrusiveList to concatenate.
            /// \return *this.
            inline IntrusiveList &operator += (IntrusiveList<T, ID> &other) {
                if (other.head != nullptr) {
                    if (tail != nullptr) {
                        next (tail) = other.head;
                        prev (other.head) = tail;
                    }
                    else {
                        head = other.head;
                    }
                    tail = other.tail;
                    count += other.count;
                    other.head = 0;
                    other.tail = 0;
                    other.count = 0;
                }
                return *this;
            }

            /// \brief
            /// Return the number of nodes in the list.
            /// \return Number of nodes in the list.
            inline std::size_t size () const {
                return count;
            }
            /// \brief
            /// Return true if list is empty.
            /// \return true if list is empty.
            inline bool empty () const {
                return count == 0;
            }

            /// \brief
            /// Return the head node.
            /// \return The head node.
            inline T *front () const {
                return head;
            }

            /// \brief
            /// Return the tail node.
            /// \return The tail node.
            inline T *back () const {
                return tail;
            }

            /// \brief
            /// Return the previous node of the given node.
            /// \param[in] node Node whose previous node to return.
            /// \return Previous node of the given node.
            inline T *&prev (T *node) const {
                assert (node != nullptr);
                return node->util::template IntrusiveList<T, ID>::Node::prev;
            }

            /// \brief
            /// Return the next node of the given node.
            /// \param[in] node Node whose next node to return.
            /// \return Next node of the given node.
            inline T *&next (T *node) const {
                assert (node != nullptr);
                return node->util::template IntrusiveList<T, ID>::Node::next;
            }

            /// \brief
            /// Return true if a given node is in this list.
            /// \param[in] node Node to check for containment.
            /// \return true if a given node is in this list.
            inline bool &contains (T *node) const {
                assert (node != nullptr);
                return node->util::template IntrusiveList<T, ID>::Node::inList;
            }

            /// \brief
            /// Add a given node to the front of the list.
            /// \param[in] node Node to add to the front of the list.
            /// \return true = Node was added to the list.
            /// false = Either node == nullptr or it's already in the list.
            inline bool push_front (T *node) {
                assert (node != nullptr);
                if (node != nullptr && !contains (node)) {
                    if (head == nullptr) {
                        assert (tail == nullptr);
                        prev (node) = next (node) = nullptr;
                        head = tail = node;
                    }
                    else {
                        prev (node) = nullptr;
                        next (node) = head;
                        head = prev (head) = node;
                    }
                    contains (node) = true;
                    ++count;
                    return true;
                }
                return false;
            }

            /// \brief
            /// Add a given node to the back of the list.
            /// \param[in] node Node to add to the back of the list.
            /// \return true = Node was added to the list.
            /// false = Either node == nullptr or it's already in the list.
            inline bool push_back (T *node) {
                assert (node != nullptr);
                if (node != nullptr && !contains (node)) {
                    if (head == nullptr) {
                        assert (tail == nullptr);
                        prev (node) = next (node) = nullptr;
                        head = tail = node;
                    }
                    else {
                        assert (tail != nullptr);
                        prev (node) = tail;
                        next (node) = nullptr;
                        tail = next (tail) = node;
                    }
                    contains (node) = true;
                    ++count;
                    return true;
                }
                return false;
            }

            /// \struct IntrusiveList::unary_function IntrusiveList.h thekogans/stream/IntrusiveList.h
            ///
            /// \brief
            /// Since this simple template has been deprecated (and now removed) from the standard library,
            /// we recreate it here as a dependency of \see{Callback} below.
            template<
                typename ArgumentType,
                typename ResultType>
            struct unary_function {
                /// \brief
                /// Expose ArgumentType template argument for derivatives to use.
                using argument_type = ArgumentType;
                /// \brief
                /// Expose ResultType template argument for derivatives to use.
                using result_type = ResultType;
            };

            /// \struct IntrusiveList::Callback IntrusiveList.h thekogans/stream/IntrusiveList.h
            ///
            /// \brief
            /// Base class for callbacks passed to clear and for_each.
            struct Callback : public unary_function<T *, bool> {
                /// \brief
                /// Alias for unary_function<T *, bool>::result_type.
                using result_type = typename unary_function<T *, bool>::result_type;
                /// \brief
                /// Alias for unary_function<T *, bool>::argument_type.
                using argument_type = typename unary_function<T *, bool>::argument_type;
                /// \brief
                /// dtor.
                virtual ~Callback () {}
                /// \brief
                /// Called by clear and for_each.
                /// \param[in] node T *.
                /// \return true = continue enumeration, false = stop enumeration.
                virtual result_type operator () (argument_type /*node*/) = 0;
            };

            /// \struct IntrusiveList::DefaultCallback IntrusiveList.h thekogans/stream/IntrusiveList.h
            ///
            /// \brief
            /// Default no op callback.
            struct DefaultCallback : public Callback {
                /// \brief
                /// Alias for Callback::result_type.
                using result_type = typename Callback::result_type;
                /// \brief
                /// Alias for Callback::argument_type.
                using argument_type = typename Callback::argument_type;
                /// \brief
                /// No op.
                virtual result_type operator () (argument_type /*node*/) override {
                    return true;
                }
            };

            /// \brief
            /// Remove all nodes from the list.
            /// \param[in] callback Callback to be called for every node in the list.
            /// VERY, VERY IMPORTANT: The node is removed from the list BEFORE the
            /// call to callback and is never touched again (regardless of the callback
            /// return value).
            /// \return true == List is cleared. false == callback returned false.
            inline bool clear (Callback &callback) {
                for (T *node = head; node != nullptr;) {
                    // After callback returns, we might not be able to access the node.
                    // Remove it from the list first.
                    T *temp = next (node);
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    if (!callback (node)) {
                        return false;
                    }
                    node = temp;
                }
                head = tail = nullptr;
                count = 0;
                return true;
            }

            /// \brief
            /// Remove all nodes from the list.
            inline void clear () {
                for (T *node = head; node != nullptr;) {
                    T *temp = next (node);
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    node = temp;
                }
                head = tail = nullptr;
                count = 0;
            }

            /// \brief
            /// Alias for std::function<bool (T * /*node*/)>.
            /// \param[in] node T *.
            /// \return true = continue enumeration, false = stop enumeration.
            using Function = std::function<bool (T * /*node*/)>;

            /// \brief
            /// Remove all nodes from the list.
            /// \param[in] callback Callback to be called for every node in the list.
            /// See VERY, VERY important comment above (clear).
            /// \return true == List is cleared. false == callback returned false.
            inline bool clear (const Function &callback) {
                for (T *node = head; node != nullptr;) {
                    // After callback returns, we might not be able to access the node.
                    // Remove it from the list first.
                    T *temp = next (node);
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    if (!callback (node)) {
                        return false;
                    }
                    node = temp;
                }
                head = tail = nullptr;
                count = 0;
                return true;
            }

            /// \brief
            /// Release the \see{RefCounted} nodes held by this list and clear it.
            inline void release () {
                clear (
                    [] (T *node) -> bool {
                        if (node != nullptr) {
                            node->Release ();
                        }
                        return true;
                    }
                );
            }

            /// \brief
            /// Reverse the nodes in the list.
            inline void reverse () {
                IntrusiveList<T, ID> list;
                for (T *node = tail; node != nullptr;) {
                    T *temp = prev (node);
                    contains (node) = false;
                    list.push_back (node);
                    node = temp;
                }
                head = tail = nullptr;
                count = 0;
                swap (list);
            }

            /// \brief
            /// Insert a given node before another.
            /// \param[in] node Node to insert.
            /// \param[in] before Node before which to insert.
            /// NOTE: before == nullptr is the same as push_back.
            /// \return true = Node was inserted in to the list.
            /// false = Either node == nullptr or it's already in the list.
            inline bool insert (
                    T *node,
                    T *before) {
                assert (node != nullptr);
                if (node != nullptr && !contains (node)) {
                    if (before == nullptr) {
                        push_back (node);
                    }
                    else {
                        next (node) = before;
                        prev (node) = prev (before);
                        if (prev (node) != nullptr) {
                            next (prev (node)) = node;
                        }
                        prev (before) = node;
                        contains (node) = true;
                        ++count;
                    }
                    return true;
                }
                return false;
            }

            /// \brief
            /// Remove a given node from the list.
            /// \param[in] node Node to remove.
            /// \return true = Node was erased from the list.
            /// false = Either node == nullptr or it's not in the list.
            inline bool erase (T *node) {
                assert (node != nullptr);
                if (node != nullptr && contains (node)) {
                    if (prev (node) != nullptr) {
                        next (prev (node)) = next (node);
                    }
                    else {
                        assert (node == head);
                        head = next (node);
                        if (head != nullptr) {
                            prev (head) = nullptr;
                        }
                    }
                    if (next (node) != nullptr) {
                        prev (next (node)) = prev (node);
                    }
                    else {
                        assert (node == tail);
                        tail = prev (node);
                        if (tail != nullptr) {
                            next (tail) = nullptr;
                        }
                    }
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    --count;
                    return true;
                }
                return false;
            }

            /// \brief
            /// Remove the head node from the list and return it.
            /// \return Head node.
            inline T *pop_front () {
                T *node = head;
                if (node != nullptr) {
                    if (next (node) != nullptr) {
                        head = next (node);
                        prev (next (node)) = nullptr;
                    }
                    else {
                        assert (head == tail);
                        assert (count == 1);
                        head = tail = nullptr;
                    }
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    --count;
                }
                return node;
            }

            /// \brief
            /// Remove the tail node from the list and return it.
            /// \return Tail node.
            inline T *pop_back () {
                T *node = tail;
                if (node != nullptr) {
                    if (prev (node) != nullptr) {
                        tail = prev (node);
                        next (prev (node)) = nullptr;
                    }
                    else {
                        assert (head == tail);
                        assert (count == 1);
                        head = tail = nullptr;
                    }
                    prev (node) = next (node) = nullptr;
                    contains (node) = false;
                    --count;
                }
                return node;
            }

            /// \brief
            /// Walk the list calling the callback for every node.
            /// The enumeration stops if callback returns false.
            /// \param[in] callback Called for every node in the list.
            /// \param[in] reverse true == Walk the list tail to head.
            /// \return true == Iterated over all elements, false == callback returned false.
            inline bool for_each (
                    Callback &callback,
                    bool reverse = false) const {
                if (reverse) {
                    for (T *node = tail; node != nullptr;) {
                        // After callback returns, we might not be able to call prev (node).
                        T *temp = prev (node);
                        if (!callback (node)) {
                            return false;
                        }
                        node = temp;
                    }
                }
                else {
                    for (T *node = head; node != nullptr;) {
                        // After callback returns, we might not be able to call next (node).
                        T *temp = next (node);
                        if (!callback (node)) {
                            return false;
                        }
                        node = temp;
                    }
                }
                return true;
            }

            /// \brief
            /// Walk the list calling the callback for every node.
            /// The enumeration stops if callback returns false.
            /// \param[in] callback Called for every node in the list.
            /// \param[in] reverse true == Walk the list tail to head.
            /// \return true == Iterated over all elements, false == callback returned false.
            inline bool for_each (
                    const Function &callback,
                    bool reverse = false) const {
                if (reverse) {
                    for (T *node = tail; node != nullptr;) {
                        // After callback returns, we might not be able to call prev (node).
                        T *temp = prev (node);
                        if (!callback (node)) {
                            return false;
                        }
                        node = temp;
                    }
                }
                else {
                    for (T *node = head; node != nullptr;) {
                        // After callback returns, we might not be able to call next (node).
                        T *temp = next (node);
                        if (!callback (node)) {
                            return false;
                        }
                        node = temp;
                    }
                }
                return true;
            }

            /// \brief
            /// Walk the list calling the callback for every node.
            /// The enumeration stops if callback returns true.
            /// \param[in] callback Called for every node in the list.
            /// \param[in] reverse true == Walk the list tail to head.
            /// \return First node for which the callback returned true.
            /// nullptr if no node was found matching the criteria.
            inline T *find (
                    Callback &callback,
                    bool reverse = false) const {
                if (reverse) {
                    for (T *node = tail; node != nullptr; node = prev (node)) {
                        if (callback (node)) {
                            return node;
                        }
                    }
                }
                else {
                    for (T *node = head; node != nullptr; node = next (node)) {
                        if (callback (node)) {
                            return node;
                        }
                    }
                }
                return nullptr;
            }

            /// \brief
            /// Walk the list calling the callback for every node.
            /// The enumeration stops if callback returns true.
            /// \param[in] callback Called for every node in the list.
            /// \param[in] reverse true == Walk the list tail to head.
            /// \return First node for which the callback returned true.
            /// nullptr if no node was found matching the criteria.
            inline T *find (
                    const Function &callback,
                    bool reverse = false) const {
                if (reverse) {
                    for (T *node = tail; node != nullptr; node = prev (node)) {
                        if (callback (node)) {
                            return node;
                        }
                    }
                }
                else {
                    for (T *node = head; node != nullptr; node = next (node)) {
                        if (callback (node)) {
                            return node;
                        }
                    }
                }
                return nullptr;
            }

            /// \brief
            /// Swap the contents of this list with the one given.
            /// \param[in] list List with which to swap contents.
            inline void swap (IntrusiveList<T, ID> &list) {
                if (this != &list) {
                    std::swap (head, list.head);
                    std::swap (tail, list.tail);
                    std::swap (count, list.count);
                }
            }

            /// \brief
            /// IntrusiveList is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (IntrusiveList)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_IntrusiveList_h)
