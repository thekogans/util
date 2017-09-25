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

#include <Foundation/Foundation.h>
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/OSXUtils.h"

namespace thekogans {
    namespace util {

        std::string DescriptionFromOSStatus (OSStatus errorCode) {
            std::string UTF8String;
            NSError *error = [NSError errorWithDomain: NSOSStatusErrorDomain code: errorCode userInfo: nil];
            if (error != 0) {
                NSString *description = [error description];
                if (description != 0) {
                    UTF8String = [description UTF8String];
                    [description release];
                }
                [error release];
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
            const ui32 descriptionTableSize = THEKOGANS_UTIL_ARRAY_SIZE (descriptionTable);
            for (ui32 i = 0; i < descriptionTableSize; ++i) {
                if (descriptionTable[i].errorCode == errorCode) {
                    return descriptionTable[i].description;
                }
            }
            return FormatString ("[0x%x:%d - Unknown error.]", errorCode, errorCode);
        }

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_OSX)
