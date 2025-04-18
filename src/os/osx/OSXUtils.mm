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

#include "thekogans/util/Environment.h"

#if defined (TOOLCHAIN_OS_OSX)

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <set>
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/SystemRunLoop.h"
#include "thekogans/util/RefCountedRegistry.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/os/osx/OSXUtils.h"

namespace thekogans {
    namespace util {

        void RunLoop::NSApplicationInitializer::InitializeWorker () noexcept {
            THEKOGANS_UTIL_TRY {
                if (NSApplicationLoad () == YES) {
                #if !__has_feature (objc_arc)
                    [NSAutoreleasePool new];
                #endif // !__has_feature (objc_arc)
                    [NSApplication sharedApplication];
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Failed to initialize NSApplication!\n");
                }
            }
            THEKOGANS_UTIL_CATCH_AND_LOG_SUBSYSTEM (THEKOGANS_UTIL)
        }

        void RunLoop::NSApplicationInitializer::UninitializeWorker () noexcept {
        }

        namespace os {
            namespace osx {

                namespace {
                    struct CFStringRefDeleter {
                        void operator () (CFStringRef stringRef) {
                            if (stringRef != 0) {
                                CFRelease (stringRef);
                            }
                        }
                    };
                    using CFStringRefPtr = std::unique_ptr<const __CFString, CFStringRefDeleter>;
                }

                std::string DescriptionFromSecOSStatus (OSStatus errorCode) {
                    CFStringRefPtr description (SecCopyErrorMessageString (errorCode, 0));
                    const char *str = nullptr;
                    if (description != nullptr) {
                        str = CFStringGetCStringPtr (description.get (), kCFStringEncodingUTF8);
                    }
                    return str != nullptr ? str : "Unknown error.";
                }

                std::string DescriptionFromOSStatus (OSStatus errorCode) {
                    std::string UTF8String;
                    NSError *error = [NSError errorWithDomain: NSOSStatusErrorDomain
                        code: errorCode userInfo: nil];
                    if (error != nullptr) {
                        NSString *description = [error description];
                        if (description != nullptr) {
                            UTF8String = [description UTF8String];
                        }
                    }
                    return !UTF8String.empty () ? UTF8String :
                        FormatString ("[0x%x:%d - Unknown error.]", errorCode, errorCode);
                }

                std::string DescriptionFromCFErrorRef (CFErrorRef error) {
                    CFStringRefPtr description (CFErrorCopyDescription (error));
                    const char *str = nullptr;
                    if (description != nullptr) {
                        str = CFStringGetCStringPtr (description.get (), kCFStringEncodingUTF8);
                    }
                    return str != nullptr ? str : "Unknown error.";
                }

                std::string DescriptionFromIOReturn (IOReturn errorCode) {
                    struct {
                        IOReturn errorCode;
                        const char *description;
                    } descriptionTable[] = {
                        {kIOReturnSuccess, "success"},
                        {kIOReturnError, "general error"},
                        {kIOReturnNoMemory, "memory allocation error"},
                        {kIOReturnNoResources, "resource shortage"},
                        {kIOReturnIPCError, "Mach IPC failure"},
                        {kIOReturnNoDevice, "no such device"},
                        {kIOReturnNotPrivileged, "privilege violation"},
                        {kIOReturnBadArgument, "invalid argument"},
                        {kIOReturnLockedRead, "device is read locked"},
                        {kIOReturnLockedWrite, "device is write locked"},
                        {kIOReturnExclusiveAccess, "device is exclusive access"},
                        {kIOReturnBadMessageID, "bad IPC message ID"},
                        {kIOReturnUnsupported, "unsupported function"},
                        {kIOReturnVMError, "virtual memory error"},
                        {kIOReturnInternalError, "internal driver error"},
                        {kIOReturnIOError, "I/O error"},
                        {kIOReturnCannotLock, "cannot acquire lock"},
                        {kIOReturnNotOpen, "device is not open"},
                        {kIOReturnNotReadable, "device is not readable"},
                        {kIOReturnNotWritable, "device is not writeable"},
                        {kIOReturnNotAligned, "alignment error"},
                        {kIOReturnBadMedia, "media error"},
                        {kIOReturnStillOpen, "device is still open"},
                        {kIOReturnRLDError, "rld failure"},
                        {kIOReturnDMAError, "DMA failure"},
                        {kIOReturnBusy, "device is busy"},
                        {kIOReturnTimeout, "I/O timeout"},
                        {kIOReturnOffline, "device is offline"},
                        {kIOReturnNotReady, "device is not ready"},
                        {kIOReturnNotAttached, "device/channel is not attached"},
                        {kIOReturnNoChannels, "no DMA channels available"},
                        {kIOReturnNoSpace, "no space for data"},
                        {kIOReturnPortExists, "device port already exists"},
                        {kIOReturnCannotWire, "cannot wire physical memory"},
                        {kIOReturnNoInterrupt, "no interrupt attached"},
                        {kIOReturnNoFrames, "no DMA frames enqueued"},
                        {kIOReturnMessageTooLarge, "message is too large"},
                        {kIOReturnNotPermitted, "operation is not permitted"},
                        {kIOReturnNoPower, "device is without power"},
                        {kIOReturnNoMedia, "media is not present"},
                        {kIOReturnUnformattedMedia, "media is not formatted"},
                        {kIOReturnUnsupportedMode, "unsupported mode"},
                        {kIOReturnUnderrun, "data underrun"},
                        {kIOReturnOverrun, "data overrun"},
                        {kIOReturnDeviceError, "device error"},
                        {kIOReturnNoCompletion, "no completion routine"},
                        {kIOReturnAborted, "operation was aborted"},
                        {kIOReturnNoBandwidth, "bus bandwidth would be exceeded"},
                        {kIOReturnNotResponding, "device is not responding"},
                        {kIOReturnInvalid, "unanticipated driver error"}
                    };
                    const std::size_t descriptionTableSize =
                        THEKOGANS_UTIL_ARRAY_SIZE (descriptionTable);
                    for (std::size_t i = 0; i < descriptionTableSize; ++i) {
                        if (descriptionTable[i].errorCode == errorCode) {
                            return descriptionTable[i].description;
                        }
                    }
                    return FormatString ("[0x%x:%d - Unknown error.]", errorCode, errorCode);
                }

                std::string GetHomeDirectory () {
                    std::string UTF8String;
                    NSString *homeDirectory = NSHomeDirectory ();
                    if (homeDirectory != nullptr) {
                        UTF8String = [homeDirectory UTF8String];
                    }
                    return UTF8String;
                }

                namespace {
                    using KQueueTimerRegistry = RefCountedRegistry<KQueueTimer>;
                }

                struct KQueueTimer : public RefCounted {
                    const ui64 id;
                    KQueueTimerCallback timerCallback;
                    void *userData;
                    const KQueueTimerRegistry::Token token;
                    bool running;

                    KQueueTimer (
                        ui64 id_,
                        KQueueTimerCallback timerCallback_,
                        void *userData_) :
                        id (id_),
                        timerCallback (timerCallback_),
                        userData (userData_),
                        token (this),
                        running (false) {}
                };

                namespace {
                    struct TimerKQueue :
                            public Singleton<TimerKQueue>,
                            public Thread {
                    private:
                        THEKOGANS_UTIL_HANDLE handle;
                        std::atomic<ui64> idPool;

                    public:
                        TimerKQueue () :
                                Thread ("TimerKQueue"),
                                handle (kqueue ()),
                                idPool (0) {
                            if (handle == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE);
                            }
                            Create (THEKOGANS_UTIL_HIGH_THREAD_PRIORITY);
                        }
                        // Considering this is a singleton, if this dtor ever gets
                        // called, we have a big problem. It's here only for show and
                        // completeness.
                        ~TimerKQueue () {
                            close (handle);
                        }

                        KQueueTimer *CreateTimer (
                                KQueueTimerCallback timerCallback,
                                void *userData) {
                            if (timerCallback != nullptr) {
                                KQueueTimer *timer = new KQueueTimer (
                                    ++idPool, timerCallback, userData);
                                // We own the timer. Call DestroyTimer below to release.
                                timer->AddRef ();
                                return timer;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }

                        void DestroyTimer (KQueueTimer *timer) {
                            if (timer != nullptr) {
                                keventStruct event;
                                keventSet (&event, timer->id, EVFILT_TIMER, EV_DELETE, 0, 0, 0);
                                keventFunc (handle, &event, 1, 0, 0, 0);
                                timer->Release ();
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }

                        void StartTimer (
                                KQueueTimer *timer,
                                const TimeSpec &timeSpec,
                                bool periodic) {
                            if (timer != nullptr) {
                                keventStruct event;
                                keventSet (
                                    &event,
                                    timer->id,
                                    EVFILT_TIMER,
                                    EV_ADD | (!periodic ? EV_ONESHOT : 0),
                                    0,
                                    timeSpec.ToMilliseconds (),
                                    timer->token.GetValue ());
                                if (keventFunc (handle, &event, 1, 0, 0, 0) == 0) {
                                    timer->running = true;
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                        THEKOGANS_UTIL_OS_ERROR_CODE);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }

                        void StopTimer (KQueueTimer *timer) {
                            if (timer != nullptr) {
                                keventStruct event;
                                keventSet (
                                    &event,
                                    timer->id,
                                    EVFILT_TIMER,
                                    EV_DELETE,
                                    0,
                                    0,
                                    timer->token.GetValue ());
                                if (keventFunc (handle, &event, 1, 0, 0, 0) == 0) {
                                    timer->running = false;
                                }
                                else {
                                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                        THEKOGANS_UTIL_OS_ERROR_CODE);
                                }
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }

                        bool IsTimerRunning (KQueueTimer *timer) {
                            if (timer != nullptr) {
                                return timer->running;
                            }
                            else {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
                            }
                        }

                    private:
                        virtual void Run () noexcept {
                            const int MaxEventsBatch = 32;
                            keventStruct kqueueEvents[MaxEventsBatch];
                            while (1) {
                                int count = keventFunc (
                                    handle, 0, 0, kqueueEvents, MaxEventsBatch, 0);
                                for (int i = 0; i < count; ++i) {
                                    KQueueTimer::SharedPtr timer =
                                        KQueueTimerRegistry::Instance ()->Get (
                                            (KQueueTimerRegistry::Token::ValueType)kqueueEvents[i].udata);
                                    if (timer != nullptr) {
                                        if ((kqueueEvents[i].flags & EV_ONESHOT) == EV_ONESHOT) {
                                            timer->running = false;
                                        }
                                        timer->timerCallback (timer->userData);
                                    }
                                }
                            }
                        }
                    };
                }

                KQueueTimer *CreateKQueueTimer (
                        KQueueTimerCallback timerCallback,
                        void *userData) {
                    return TimerKQueue::Instance ()->CreateTimer (timerCallback, userData);
                }

                void DestroyKQueueTimer (KQueueTimer *timer) {
                    TimerKQueue::Instance ()->DestroyTimer (timer);
                }

                void StartKQueueTimer (
                        KQueueTimer *timer,
                        const TimeSpec &timeSpec,
                        bool periodic) {
                    TimerKQueue::Instance ()->StartTimer (timer, timeSpec, periodic);
                }

                void StopKQueueTimer (KQueueTimer *timer) {
                    TimerKQueue::Instance ()->StopTimer (timer);
                }

                bool IsKQueueTimerRunning (KQueueTimer *timer) {
                    return TimerKQueue::Instance ()->IsTimerRunning (timer);
                }

                namespace {
                    struct CFRunLoopSourceRefDeleter {
                        void operator () (CFRunLoopSourceRef runLoopSourceRef) {
                            if (runLoopSourceRef != 0) {
                                CFRelease (runLoopSourceRef);
                            }
                        }
                    };
                    using CFRunLoopSourceRefPtr =
                        std::unique_ptr<__CFRunLoopSource, CFRunLoopSourceRefDeleter>;

                    void DoNothingRunLoopCallback (void * /*info*/) {
                    }
                }

                void CFRunLoop::Begin () {
                    // Create a dummy source so that CFRunLoopRun has
                    // something to wait on.
                    CFRunLoopSourceContext context = {0};
                    context.perform = DoNothingRunLoopCallback;
                    CFRunLoopSourceRefPtr runLoopSource (CFRunLoopSourceCreate (0, 0, &context));
                    if (runLoopSource != nullptr) {
                        CFRunLoopAddSource (runLoop, runLoopSource.get (), kCFRunLoopCommonModes);
                        CFRunLoopRun ();
                        CFRunLoopRemoveSource (runLoop, runLoopSource.get (), kCFRunLoopCommonModes);
                    }
                    else {
                        THEKOGANS_UTIL_THROW_SC_ERROR_CODE_EXCEPTION (SCError ());
                    }
                }

                void CFRunLoop::End () {
                    CFRunLoopStop (runLoop);
                }

                void NSAppRunLoop::Begin () {
                    [NSApp run];
                }

                void NSAppRunLoop::End () {
                    [NSApp stop: nil];
                    // Inject an event, so that the loop actually checks
                    // the STOP flag set above and exits the loop.
                    NSEvent *event = [NSEvent
                        otherEventWithType: NSEventTypeApplicationDefined
                        location: NSMakePoint (0.0f, 0.0f)
                        modifierFlags: 0
                        timestamp: 0.0f
                        windowNumber: 0
                        context: nil
                        subtype: 0
                        data1: 0
                        data2: 0];
                    [NSApp postEvent: event atStart: NO];
                }

            } // namespace osx
        } // namespace os
    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_OSX)
