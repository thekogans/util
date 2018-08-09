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

#if defined (TOOLCHAIN_OS_OSX)

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include "thekogans/util/Exception.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/OSXUtils.h"

namespace thekogans {
    namespace util {

        void CocoaInit () {
            if (NSApplicationLoad () == YES) {
                [NSAutoreleasePool new];
                [NSApplication sharedApplication];
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "%s", "Failed to initialize Cocoa!\n");
            }
        }

        void CocoaStart () {
            [NSApp run];
        }

        void CocoaStop () {
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

        std::string DescriptionFromOSStatus (OSStatus errorCode) {
            std::string UTF8String;
            NSError *error = [NSError errorWithDomain: NSOSStatusErrorDomain code: errorCode userInfo: nil];
            if (error != 0) {
                NSString *description = [error description];
                if (description != 0) {
                    UTF8String = [description UTF8String];
                #if defined (__clang__) && !__has_feature (objc_arc)
                    [description release];
                #endif // defined (__clang__) && !__has_feature (objc_arc)
                }
            #if defined (__clang__) && !__has_feature (objc_arc)
                [error release];
            #endif // defined (__clang__) && !__has_feature (objc_arc)
            }
            return !UTF8String.empty () ? UTF8String :
                FormatString ("[0x%x:%d - Unknown error.]", errorCode, errorCode);
        }

        namespace {
            struct CFStringRefDeleter {
                void operator () (CFStringRef stringRef) {
                    if (stringRef != 0) {
                        CFRelease (stringRef);
                    }
                }
            };
            typedef std::unique_ptr<const __CFString, CFStringRefDeleter> CFStringRefPtr;
        }

        std::string DescriptionFromCFErrorRef (CFErrorRef error) {
            CFStringRefPtr description (CFErrorCopyDescription (error));
            const char *str = 0;
            if (description.get () != 0) {
                str = CFStringGetCStringPtr (description.get (), kCFStringEncodingUTF8);
            }
            return str != 0 ? str : "Unknown error.";
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
            const std::size_t descriptionTableSize = THEKOGANS_UTIL_ARRAY_SIZE (descriptionTable);
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
            if (homeDirectory != 0) {
                UTF8String = [homeDirectory UTF8String];
            #if defined (__clang__) && !__has_feature (objc_arc)
                [homeDirectory release];
            #endif // defined (__clang__) && !__has_feature (objc_arc)
            }
            return UTF8String;
        }

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_OSX)
