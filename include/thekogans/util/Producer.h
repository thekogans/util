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

#if !defined (__thekogans_util_Producer_h)
#define __thekogans_util_Producer_h

#include <functional>
#include <map>
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/JobQueue.h"

namespace thekogans {
    namespace util {

        /// \brief
        /// Forward declaration of \see{Subscriber}.
        template<typename T>
        struct Subscriber;

        /// \struct Producer Producer.h thekogans/util/Producer.h
        ///
        /// \brief
        /// Together with \see{Subscriber}, Producer implements a producer/subscriber pattern.
        /// Here's a simple use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct Events1 {
        ///     virtual ~Events1 () {}
        ///
        ///     virtual void Ping (int count) {}
        /// };
        ///
        /// struct Events2 {
        ///     virtual ~Events2 () {}
        ///
        ///     virtual void Pong () {}
        /// };
        ///
        /// struct Producer :
        ///         public Singleton<
        ///             Producer,
        ///             SpinLock,
        ///             util::RefCountedInstanceCreator<Producer>,
        ///             util::RefCountedInstanceDestroyer<Producer>>,
        ///         public util::Producer<Events1>,
        ///         public util::Producer<Events2> {
        ///     void foo () {
        ///         // Do some work.
        ///         ...
        ///         // Emit event.
        ///         util::Producer<Events1>::Produce (std::bind (&Events1::Ping, std::placeholders::_1, 5));
        ///     }
        ///
        ///     void bar () {
        ///         // Do some work.
        ///         ...
        ///         // Emit event.
        ///         util::Producer<Events2>::Produce (std::bind (&Events2::Pong, std::placeholders::_1));
        ///     }
        /// };
        /// \endcode

        template <typename T>
        struct Producer : public virtual RefCounted {
            /// \brief
            /// Declare \see{RefCounted} pointers.
            THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (Producer<T>)

            /// \struct Producer::EventDeliveryPolicy Producer.h thekogans/util/Producer.h
            ///
            /// \brief
            /// An abstract base class encapsulating the mechanism by which events
            /// are delivered to subscribers.
            struct EventDeliveryPolicy : public RefCounted {
                /// \brief
                /// Convenient typedef for RefCounted::SharedPtr<EventDeliveryPolicy>>.
                typedef RefCounted::SharedPtr<EventDeliveryPolicy> SharedPtr;

                /// \brief
                /// dtor.
                virtual ~EventDeliveryPolicy () {}

                /// \brief
                /// Must be overridden by concrete classes to deliver events using whatever
                /// means are appropriate to them.
                /// \param[in] event Event to deliver.
                /// \param[in] subscriber \see{Subscriber} to whom to deliver the event.
                virtual void DeliverEvent (
                    std::function<void (T *)> event,
                    typename Subscriber<T>::SharedPtr subscriber) = 0;
            };

            /// \struct Producer::ImmediateEventDeliveryPolicy Producer.h thekogans/util/Producer.h
            ///
            /// \brief
            /// Delivers the event immediately to the subscriber.
            /// NOTE: The event is being delivered while the producer's subscribers is held.
            struct ImmediateEventDeliveryPolicy : public EventDeliveryPolicy {
                /// \brief
                /// Deliver the given event immediately to the given subscriber.
                /// \param[in] event Event to deliver.
                /// \param[in] subscriber \see{Subscriber} to whom to deliver the event.
                virtual void DeliverEvent (
                        std::function<void (T *)> event,
                        typename Subscriber<T>::SharedPtr subscriber) {
                    event (subscriber.Get ());
                }
            };

            /// \struct Producer::RunLoopEventDeliveryPolicy Producer.h thekogans/util/Producer.h
            ///
            /// \brief
            /// Queue a \see{RunLoop} lambda job that will deliver the given event to the given
            /// subscriber when the job is executed by the run loop.
            struct RunLoopEventDeliveryPolicy : public EventDeliveryPolicy {
                /// \brief
                /// \see{RunLoop} on which to queue the event delivery job.
                RunLoop &runLoop;

                /// \brief
                /// ctor.
                /// \param[in] runLoop_ \see{RunLoop} on which to queue the event delivery job.
                explicit RunLoopEventDeliveryPolicy (RunLoop &runLoop_) :
                    runLoop (runLoop_) {}

                /// \brief
                /// Deliver the given event to the given subscriber by queueing a job
                /// on the contained \see{RunLoop}.
                /// \param[in] event Event to deliver.
                /// \param[in] subscriber \see{Subscriber} to whom to deliver the event.
                virtual void DeliverEvent (
                        std::function<void (T *)> event,
                        typename Subscriber<T>::SharedPtr subscriber) {
                    auto job = [event, subscriber] (
                            RunLoop::Job & /*job*/,
                            const std::atomic<bool> & /*done*/) {
                        event (subscriber.Get ());
                    };
                    runLoop.EnqJob (job);
                }
            };

            /// \struct Producer::JobQueueEventDeliveryPolicy Producer.h thekogans/util/Producer.h
            ///
            /// \brief
            /// Gives each \see{Subscrivber} it's own delivery \see{JobQueue}.
            struct JobQueueEventDeliveryPolicy : public RunLoopEventDeliveryPolicy {
                /// \brief
                /// \see{JobQueue} on which to queue the event delivery job.
                JobQueue jobQueue;

                /// \brief
                /// ctor.
                JobQueueEventDeliveryPolicy () :
                    RunLoopEventDeliveryPolicy (jobQueue) {}
            };

        protected:
            /// \brief
            /// Convenient typedef for std::pair<typename Subscriber<T>::WeakPtr *,
            /// typename EventDeliveryPolicy::SharedPtr>.
            typedef std::pair<
                typename Subscriber<T>::WeakPtr *,
                typename EventDeliveryPolicy::SharedPtr> SubscriberInfo;
            /// \brief
            /// Convenient typedef for std::map<Subscriber<T> *, SubscriberInfo>.
            typedef std::map<Subscriber<T> *, SubscriberInfo> Subscribers;
            /// \brief
            /// Map of registered subscribers.
            Subscribers subscribers;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \nrief
            /// dtor.
            virtual ~Producer () {
                // We're going out of scope, delete all subscribers.
                LockGuard<SpinLock> guard (spinLock);
                for (typename Subscribers::iterator
                        it = subscribers.begin (),
                        end = subscribers.end (); it != end; ++it) {
                    delete it->second.first;
                }
                subscribers.clear ();
            }

            /// \brief
            /// Called by \see{Subscriber} to add itself to the subscribers map.
            /// \param[in] subscriber \see{Subscriber} to add to the subscribers map.
            /// \param[in] eventDeliveryPolicy \see{EventDeliveryPolicy} by which events are delivered.
            /// NOTE: We respect the fact that some producers are designed for lazy initialization
            /// by waiting for the first subscriber before commiting potentially expensive resources.
            /// That's why we call OnSubscribe under lock to make sure the count they get acurately
            /// reflects that fact. It's important that you don't call back in to the Producer as
            /// deadlock will occur.
            void Subscribe (
                    Subscriber<T> &subscriber,
                    typename EventDeliveryPolicy::SharedPtr eventDeliveryPolicy) {
                LockGuard<SpinLock> guard (spinLock);
                typename Subscribers::iterator it = subscribers.find (&subscriber);
                if (it == subscribers.end ()) {
                    subscribers.insert (
                        typename Subscribers::value_type (
                            &subscriber,
                            SubscriberInfo (
                                // NOTE: We store a WeakPtr pointer because we can't risk a WeakPtr
                                // copy ctor being called. It uses GetSharedPtr and that will be
                                // problematic for objects subscribing in their ctors (shared == 0).
                                new typename Subscriber<T>::WeakPtr (&subscriber),
                                eventDeliveryPolicy)));
                    OnSubscribe (subscriber, eventDeliveryPolicy, subscribers.size ());
                }
            }

            /// \brief
            /// Called by \see{Subscriber} to remove itself from the subscribers map.
            /// \param[in] subscriber \see{Subscriber} to remove from the subscribers map.
            /// NOTE: We respect the fact that some producers are designed for lazy cleanup
            /// by waiting for the last subscriber to unsubscribe before deallocating resources.
            /// That's why we call OnUnsubscribe under lock to make sure the count they get acurately
            /// reflects that fact. It's important that you don't call back in to the Producer as
            /// deadlock will occur.
            void Unsubscribe (Subscriber<T> &subscriber) {
                LockGuard<SpinLock> guard (spinLock);
                typename Subscribers::iterator it = subscribers.find (&subscriber);
                if (it != subscribers.end ()) {
                    delete it->second.first;
                    subscribers.erase (it);
                    OnUnsubscribe (subscriber, subscribers.size ());
                }
            }

            /// \brief
            /// Produce an event for subscribers to consume.
            /// \param[in] event Event to deliver to all registered subscribers.
            void Produce (std::function<void (T *)> event) {
                Subscribers subscribers_;
                {
                    // Copy the subscribers map in to a local
                    // variable before delivering the event in case
                    // they want to unsubscribe while processing it.
                    LockGuard<SpinLock> guard (spinLock);
                    subscribers_ = subscribers;
                }
                for (typename Subscribers::iterator
                        it = subscribers_.begin (),
                        end = subscribers_.end (); it != end; ++it) {
                    // NOTE: If we get a NULL pointer here it simply means that that particular subscriber
                    // is in the porocess of deallocating. It just hasn't removed itself from our subscriber
                    // list (~Subscriber) in time for us to include it in subscribers_ above.
                    // This race is unavoidable but harmless. We want to preserve the right of
                    // the \see{Subscriber} to be able to call back in to the producer while
                    // processing a particular event.
                    typename Subscriber<T>::SharedPtr subscriber = it->second.first->GetSharedPtr ();
                    if (subscriber.Get () != 0) {
                        it->second.second->DeliverEvent (event, subscriber);
                    }
                }
            }

            /// \brief
            /// Return the count of registered subscribers.
            /// \return The count of registered subscribers.
            std::size_t GetSubscriberCount () {
                LockGuard<SpinLock> guard (spinLock);
                return subscribers.size ();
            }

            /// \brief
            /// Override this methid to react to a new \see{Subscriber}.
            /// \param[in] subscriber \see{Subscriber} to add to the subscribers list.
            /// \param[in] eventDeliveryPolicy \see{EventDeliveryPolicy} by which events are delivered.
            /// \param[in] subscriberCount Number of \see{Subscriber}s (including this one).
            virtual void OnSubscribe (
                Subscriber<T> & /*subscriber*/,
                typename EventDeliveryPolicy::SharedPtr /*eventDeliveryPolicy*/,
                std::size_t /*subscriberCount*/) {}
            /// \brief
            /// Override this methid to react to a \see{Subscriber} being removed.
            /// \param[in] subscriber \see{Subscriber} to remove from the subscribers list.
            /// \param[in] subscriberCount Number of \see{Subscriber}s remaining.
            virtual void OnUnsubscribe (
                Subscriber<T> & /*subscriber*/,
                std::size_t /*subscriberCount*/) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Subscriber_h)
