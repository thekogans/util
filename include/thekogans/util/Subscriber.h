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

#if !defined (__thekogans_util_Subscriber_h)
#define __thekogans_util_Subscriber_h

#include <unordered_map>
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Producer.h"

namespace thekogans {
    namespace util {

        /// \struct Subscriber Subscriber.h thekogans/util/Subscriber.h
        ///
        /// \brief
        /// Together with \see{Producer}, Subscriber implements a producer/subscriber pattern.
        /// Here's a simple use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct Subscriber :
        ///         public util::Subscriber<Events1>,
        ///         public util::Subscriber<Events2> {
        ///     Subscriber (Producer &producer) {
        ///         util::Subscriber<Events1>::Subscribe (producer);
        ///         util::Subscriber<Events2>::Subscribe (producer);
        ///     }
        /// };
        /// \endcode

        template <typename T>
        struct Subscriber :
                public virtual RefCounted,
                public T {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Subscriber<T>)

            /// \brief
            /// default ctor.
            Subscriber () {}
            /// \brief
            /// dtor.
            virtual ~Subscriber () {
                // We're going out of scope. If we're subscribed, those producers
                // will eventually realize that we're gone (Producer::GetSubscribers)
                // and remove us.
            }

            /// \brief
            /// Return true if we're subscribed to the given producer.
            /// \param[in] producer \see{Producer} to check for subscription.
            /// \return true == We're subscribed to the given producer.
            inline bool IsSubscribed (Producer<T> &producer) {
                return producer.IsSubscribed (*this);
            }

            /// \brief
            /// Given a \see{Producer} of particular events, subscribe to them.
            /// \param[in] producer \see{Producer} whose events we want to subscribe to.
            /// \param[in] eventDeliveryPolicy \see{Producer::EventDeliveryPolicy}
            /// by which events are delivered.
            /// \return true == subscribed, false == already subscribed.
            inline bool Subscribe (
                    Producer<T> &producer,
                    typename Producer<T>::EventDeliveryPolicy::SharedPtr eventDeliveryPolicy =
                        typename Producer<T>::EventDeliveryPolicy::SharedPtr (
                            new typename Producer<T>::ImmediateEventDeliveryPolicy)) {
                return producer.Subscribe (*this, eventDeliveryPolicy);
            }

            /// \brief
            /// Given a \see{Producer} of particular events, unsubscribe from it.
            /// \param[in] producer \see{Producer} whose events we want to unsubscribe from.
            /// \return true == unsubscribed, false == was not subscribed.
            inline bool Unsubscribe (Producer<T> &producer) {
                return producer.Unsubscribe (*this);
            }

            /// \brief
            /// Subscriber is neither copy or move constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_MOVE_AND_ASSIGN (Subscriber)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Subscriber_h)
