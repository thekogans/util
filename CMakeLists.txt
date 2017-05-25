if(WIN32)
    add_definitions(
        -DTOOLCHAIN_OS_Windows
        -D_LIB_THEKOGANS_UTIL_BUILD)
    remove_definitions(
        -DUNICODE
        -D_UNICODE)
endif()

if(APPLE)
    add_definitions(-DTOOLCHAIN_OS_OSX)
endif()

add_definitions(
    -DTOOLCHAIN_ARCH_x86_64
    -DTHEKOGANS_UTIL_MAJOR_VERSION=0
    -DTHEKOGANS_UTIL_MINOR_VERSION=16
    -DTHEKOGANS_UTIL_PATCH_VERSION=0)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DTOOLCHAIN_CONFIG_Debug)
else()
    add_definitions(-DTOOLCHAIN_CONFIG_Release)
endif()

file(GLOB_RECURSE HEADERS "include/thekogans/util/*.h")
set(SOURCES
        src/AlignedAllocator.cpp
        src/Barrier.cpp
        src/Base64.cpp
        src/BitSet.cpp
        src/Buffer.cpp
        src/CRC32.cpp
        src/ChildProcess.cpp
        src/CommandLineOptions.cpp
        src/Condition.cpp
        src/Console.cpp
        src/ConsoleLogger.cpp
        src/DefaultAllocator.cpp
        src/DefaultRunLoop.cpp
        src/Directory.cpp
        src/DynamicLibrary.cpp
        src/Event.cpp
        src/Exception.cpp
        src/File.cpp
        src/FileLogger.cpp
        src/Fraction.cpp
        src/GUID.cpp
        src/HRTimer.cpp
        src/HRTimerMgr.cpp
        src/Hash.cpp
        src/Heap.cpp
        src/JobQueue.cpp
        src/JobQueueScheduler.cpp
        src/LoggerMgr.cpp
        src/MainRunLoop.cpp
        src/MD5.cpp
        src/MimeTypeMapper.cpp
        src/Mutex.cpp
        src/NullAllocator.cpp
        src/NullLock.cpp
        src/Path.cpp
        src/Pipeline.cpp
        src/RWLock.cpp
        src/RandomSource.cpp
        src/SHA1.cpp
        src/SHA2.cpp
        src/SHA2_224_256.cpp
        src/SHA2_384_512.cpp
        src/SHA3.cpp
        src/Scheduler.cpp
        src/SecureAllocator.cpp
        src/Semaphore.cpp
        src/Serializer.cpp
        src/SharedAllocator.cpp
        src/SpinLock.cpp
        src/SpinRWLock.cpp
        src/StringUtils.cpp
        src/SystemInfo.cpp
        src/SystemRunLoop.cpp
        src/Thread.cpp
        src/TimeSpec.cpp
        src/Timer.cpp
        src/Variant.cpp
        src/Vectorizer.cpp
        src/Version.cpp
        src/WorkerPool.cpp
        src/XMLUtils.cpp
        src/internal.cpp)

if(APPLE)
    list(APPEND SOURCES src/NSLogLogger.cpp)
endif()

if(WIN32)
    list(APPEND SOURCES src/OutputDebugStringLogger.cpp)
endif()

add_library(thekogans_util ${HEADERS} ${SOURCES})

if(WIN32)
      target_link_libraries(thekogans_util WS2_32)
endif()

# Link against the dependencies defined by Kragle, if this library is used in kragle
# You might want to setup your own linkage for your project, dear user of the Util library...
if(KRAGLE_DEPS)
target_link_libraries(thekogans_util
    boost_lib
	zlib_lib)
endif()

target_include_directories(thekogans_util PUBLIC include)
