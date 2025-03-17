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
#include <unordered_map>
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
        ///         public util::RefCountedSingleton<Producer>,
        ///         public util::Producer<Events1>,
        ///         public util::Producer<Events2> {
        ///     void foo () {
        ///         // Do some work.
        ///         ...
        ///         // Emit event.
        ///         util::Producer<Events1>::Produce (
        ///             std::bind (
        ///                 &Events1::Ping,
        ///                 std::placeholders::_1,
        ///                 5));
        ///     }
        ///
        ///     void bar () {
        ///         // Do some work.
        ///         ...
        ///         // Emit event.
        ///         util::Producer<Events2>::Produce (
        ///             std::bind (
        ///                 &Events2::Pong,
        ///                 std::placeholders::_1));
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
            struct EventDeliveryPolicy : public virtual RefCounted {
                /// \brief
                /// Declare \see{RefCounted} pointers.
                THEKOGANS_UTIL_DECLARE_REF_COUNTED_POINTERS (EventDeliveryPolicy)

                /// \brief
                /// Alias for std::function<void (T *)>.
                using Event = std::function<void (T *)>;


                /// \brief
                /// Must be overridden by concrete classes to deliver events using whatever
                /// means are appropriate to them.
                /// \param[in] event Event to deliver.
                /// \param[in] subscriber \see{Subscriber} to whom to deliver the event.
                virtual void DeliverEvent (
                    const Event &event,
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
                        const typename EventDeliveryPolicy::Event &event,
                        typename Subscriber<T>::SharedPtr subscriber) override {
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
                RunLoop::SharedPtr runLoop;

                /// \brief
                /// ctor.
                /// \param[in] runLoop_ \see{RunLoop} on which to queue the event delivery job.
                explicit RunLoopEventDeliveryPolicy (RunLoop::SharedPtr runLoop_) :
                    runLoop (runLoop_) {}

                /// \brief
                /// Deliver the given event to the given subscriber by queueing a job
                /// on the contained \see{RunLoop}.
                /// \param[in] event Event to deliver.
                /// \param[in] subscriber \see{Subscriber} to whom to deliver the event.
                virtual void DeliverEvent (
                        const typename EventDeliveryPolicy::Event &event,
                        typename Subscriber<T>::SharedPtr subscriber) override {
                    runLoop->EnqJob (
                        [event, subscriber] (
                                const RunLoop::LambdaJob &job,
                                const std::atomic<bool> &done) {
                            if (job.IsRunning (done)) {
                                event (subscriber.Get ());
                            }
                        });
                }
            };

            /// \struct Producer::JobQueueEventDeliveryPolicy Producer.h thekogans/util/Producer.h
            ///
            /// \brief
            /// Gives each \see{Subscriber} it's own delivery \see{JobQueue}.
            struct JobQueueEventDeliveryPolicy : public RunLoopEventDeliveryPolicy {
                /// \brief
                /// ctor.
                /// \param[in] name \see{JobQueue} name. If set, \see{JobQueue::State::Worker}
                /// threads will be named name-%d.
                /// \param[in] jobExecutionPolicy JobQueue \see{RunLoop::JobExecutionPolicy}.
                /// \param[in] maxPendingJobs Max pending queue jobs.
                /// \param[in] workerCount Max workers to service the queue.
                /// \param[in] workerPriority Worker thread priority.
                /// \param[in] workerAffinity Worker thread processor affinity.
                /// \param[in] workerCallback Called to initialize/uninitialize
                /// the worker thread(s).
                JobQueueEventDeliveryPolicy (
                    const std::string &name = std::string (),
                    RunLoop::JobExecutionPolicy::SharedPtr jobExecutionPolicy =
                        new RunLoop::FIFOJobExecutionPolicy,
                    std::size_t workerCount = 1,
                    i32 workerPriority = THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY,
                    ui32 workerAffinity = THEKOGANS_UTIL_MAX_THREAD_AFFINITY,
                    JobQueue::WorkerCallback *workerCallback = nullptr) :
                    RunLoopEventDeliveryPolicy (
                        new JobQueue (
                            name,
                            jobExecutionPolicy,
                            workerCount,
                            workerPriority,
                            workerAffinity,
                            workerCallback)) {}
            };

        private:
            /// \brief
            /// Alias for std::pair<typename Subscriber<T>::WeakPtr,
            /// typename EventDeliveryPolicy::SharedPtr>.
            using SubscriberInfo = std::pair<
                typename Subscriber<T>::WeakPtr,
                typename EventDeliveryPolicy::SharedPtr>;
            /// \brief
            /// Alias for std::unordered_map<Subscriber<T> *, SubscriberInfo>.
            using Subscribers = std::unordered_map<Subscriber<T> *, SubscriberInfo>;
            /// \brief
            /// Map of registered subscribers.
            Subscribers subscribers;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;

        public:
            /// \brief
            /// dtor.
            virtual ~Producer () {
                // We're going out of scope, delete all subscribers.
                Unsubscribe ();
            }

            /// \brief
            /// Called by \see{Subscriber} to add itself to the subscribers map.
            /// \param[in] subscriber \see{Subscriber} to add to the subscribers map.
            /// \param[in] eventDeliveryPolicy \see{EventDeliveryPolicy} by which
            /// events are delivered.
            /// \return true == subscribed, false == already subscribed.
            bool Subscribe (
                    Subscriber<T> &subscriber,
                    typename EventDeliveryPolicy::SharedPtr eventDeliveryPolicy =
                        new ImmediateEventDeliveryPolicy) {
                {
                    LockGuard<SpinLock> guard (spinLock);
                    typename Subscribers::iterator it = subscribers.find (&subscriber);
                    if (it != subscribers.end ()) {
                        return false;
                    }
                    subscribers.insert (
                        typename Subscribers::value_type (
                            &subscriber,
                            SubscriberInfo (
                                typename Subscriber<T>::WeakPtr (&subscriber),
                                eventDeliveryPolicy)));
                }
                OnSubscribe (subscriber, eventDeliveryPolicy);
                return true;
            }

            /// \brief
            /// Called by \see{Subscriber} to remove itself from the subscribers map.
            /// \param[in] subscriber \see{Subscriber} to remove from the subscribers map.
            /// \return true == unsubscribed, false == was not subscribed.
            bool Unsubscribe (Subscriber<T> &subscriber) {
                {
                    LockGuard<SpinLock> guard (spinLock);
                    typename Subscribers::iterator it = subscribers.find (&subscriber);
                    if (it == subscribers.end ()) {
                        return false;
                    }
                    subscribers.erase (it);
                }
                OnUnsubscribe (subscriber);
                return true;
            }

            /// \brief
            /// Unsubscribe all subscribers.
            void Unsubscribe () {
                Subscribers subscribers_;
                {
                    // Copy the subscribers map in to a local
                    // variable before calling OnUnsubscribe in case
                    // they want to unsubscribe while processing it.
                    LockGuard<SpinLock> guard (spinLock);
                    subscribers_.swap (subscribers);
                }
                for (typename Subscribers::iterator
                        it = subscribers_.begin (),
                        end = subscribers_.end (); it != end; ++it) {
                    // NOTE: If we get a NULL pointer here it simply means
                    // that that particular subscriber is in the porocess
                    // of deallocating. It just hasn't removed itself from
                    // our subscriber list (~Subscriber) in time for us to
                    // include it in subscribers_ above. This race is unavoidable
                    // but harmless. We want to preserve the right of the
                    // \see{Subscriber} to be able to call back in to the
                    // producer while processing a particular event.
                    typename Subscriber<T>::SharedPtr subscriber =
                        it->second.first.GetSharedPtr ();
                    if (subscriber != nullptr) {
                        OnUnsubscribe (*subscriber);
                    }
                }
            }

            /// \brief
            /// Produce an event for subscribers to consume.
            /// \param[in] event Event to deliver to all registered subscribers.
            void Produce (const typename EventDeliveryPolicy::Event &event) {
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
                    // NOTE: If we get a NULL pointer here it simply means
                    // that that particular subscriber is in the porocess
                    // of deallocating. It just hasn't removed itself from
                    // our subscriber list (~Subscriber) in time for us to
                    // include it in subscribers_ above. This race is unavoidable
                    // but harmless. We want to preserve the right of the
                    // \see{Subscriber} to be able to call back in to the
                    // producer while processing a particular event.
                    typename Subscriber<T>::SharedPtr subscriber =
                        it->second.first.GetSharedPtr ();
                    if (subscriber != nullptr) {
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
            /// \param[in] eventDeliveryPolicy \see{EventDeliveryPolicy} by
            /// which events are delivered.
            virtual void OnSubscribe (
                Subscriber<T> & /*subscriber*/,
                typename EventDeliveryPolicy::SharedPtr /*eventDeliveryPolicy*/) {}
            /// \brief
            /// Override this methid to react to a \see{Subscriber} being removed.
            /// \param[in] subscriber \see{Subscriber} to remove from the subscribers list.
            virtual void OnUnsubscribe (Subscriber<T> & /*subscriber*/) {}
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Subscriber_h)
