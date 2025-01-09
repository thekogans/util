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

#include <map>
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

        private:
            /// \brief
            /// Alias for std::map<Producer<T> *, typename Producer<T>::WeakPtr>.
            using Producers = std::map<Producer<T> *, typename Producer<T>::WeakPtr>;
            /// \brief
            /// List of producers whos events we subscribe to.
            Producers producers;
            /// \brief
            /// Synchronization lock for producers.
            /// NOTE: In a multi-threaded/async environment there may be a need
            /// for delayed subscription coming from multiple threads. This lock
            /// is here for that reason.
            SpinLock spinLock;

        public:
            /// \brief
            /// dtor.
            virtual ~Subscriber () {
                // We're going out of scope, unsubscribe ourselves from our producer's events.
                Unsubscribe ();
            }

            /// \brief
            /// Return true if we're subscribed to the given producer.
            /// \param[in] producer \see{Producer} to check for subscription.
            /// \return true == We're subscribed to the given producer.
            bool IsSubscribed (Producer<T> &producer) {
                LockGuard<SpinLock> guard (spinLock);
                return producers.find (&producer) != producers.end ();
            }

            /// \brief
            /// Given a \see{Producer} of particular events, subscribe to them.
            /// \param[in] producer \see{Producer} whose events we want to subscribe to.
            /// \param[in] eventDeliveryPolicy \see{Producer::EventDeliveryPolicy}
            /// by which events are delivered.
            /// \return true == subscribed, false == already subscribed.
            bool Subscribe (
                    Producer<T> &producer,
                    typename Producer<T>::EventDeliveryPolicy::SharedPtr eventDeliveryPolicy =
                        typename Producer<T>::EventDeliveryPolicy::SharedPtr (
                            new typename Producer<T>::ImmediateEventDeliveryPolicy)) {
                LockGuard<SpinLock> guard (spinLock);
                if (producer.Subscribe (*this, eventDeliveryPolicy)) {
                    producers.insert (
                        typename Producers::value_type (
                            &producer,
                            typename Producer<T>::WeakPtr (&producer)));
                    return true;
                }
                return false;
            }

            /// \brief
            /// Given a \see{Producer} of particular events, unsubscribe from it.
            /// \param[in] producer \see{Producer} whose events we want to unsubscribe from.
            /// \return true == unsubscribed, false == was not subscribed.
            bool Unsubscribe (Producer<T> &producer) {
                LockGuard<SpinLock> guard (spinLock);
                typename Producers::iterator it = producers.find (&producer);
                if (it != producers.end ()) {
                    producer.Unsubscribe (*this);
                    producers.erase (it);
                    return true;
                }
                return false;
            }

            /// \brief
            /// Unsubscribe from all \see{Producer}s of particular events.
            void Unsubscribe () {
                LockGuard<SpinLock> guard (spinLock);
                for (typename Producers::iterator
                        it = producers.begin (),
                        end = producers.end (); it != end; ++it) {
                    typename Producer<T>::SharedPtr producer = it->second.GetSharedPtr ();
                    if (producer != nullptr) {
                        producer->Unsubscribe (*this);
                    }
                }
                producers.clear ();
            }
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Subscriber_h)
