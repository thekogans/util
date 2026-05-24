cmake_minimum_required(VERSION 3.15)

set(namespace "thekogans")
project("thekogans_pathfinder"
    VERSION 0.37.1
    DESCRIPTION "thekogans util pathfinder example."
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME})

find_library(PTHREAD_LIB pthread)
find_library(FOUNDATION_FRAMEWORK Foundation)
find_library(SECURITY_FRAMEWORK Security)
find_library(COCOA_FRAMEWORK Cocoa)
find_library(COCOA_FOUNDATION_FRAMEWORK CoreFoundation)
find_library(CORE_SERVICES_FRAMEWORK CoreServices)
find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
find_library(SYSTEM_CONFIGURATION_FRAMEWORK SystemConfiguration)
find_library(IOKIT_FRAMEWORK IOKit)

add_definitions(
    -DTOOLCHAIN_OS_OSX
    -DTOOLCHAIN_COMPILER_clang
    -DTHEKOGANS_UTIL_HAVE_MMAP
    -DTOOLCHAIN_ARCH_x86_64
    -DTOOLCHAIN_ENDIAN_Little
    -DTHEKOGANS_PATHFINDER_MAJOR_VERSION=0
    -DTHEKOGANS_PATHFINDER_MINOR_VERSION=2
    -DTHEKOGANS_PATHFINDER_PATCH_VERSION=0
    -DTHEKOGANS_PATHFINDER_VERSION=0x00000200
    -DTHEKOGANS_UTIL_MAJOR_VERSION=0
    -DTHEKOGANS_UTIL_MINOR_VERSION=37
    -DTHEKOGANS_UTIL_PATCH_VERSION=1
    -DTHEKOGANS_UTIL_VERSION=0x00002501
    -DTHEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC
    -DTOOLCHAIN_CONFIG_Debug
    -D_DEBUG
    -D_LIB_THEKOGANS_UTIL_BUILD
    -DTHEKOGANS_UTIL_CONFIG_Debug
    -DTHEKOGANS_UTIL_TYPE_Static)

target_sources(${PROJECT_NAME}
    PRIVATE
        src/3rdparty/zlib/adler32.c
        src/3rdparty/zlib/compress.c
        src/3rdparty/zlib/crc32.c
        src/3rdparty/zlib/deflate.c
        src/3rdparty/zlib/gzclose.c
        src/3rdparty/zlib/gzlib.c
        src/3rdparty/zlib/gzread.c
        src/3rdparty/zlib/gzwrite.c
        src/3rdparty/zlib/infback.c
        src/3rdparty/zlib/inffast.c
        src/3rdparty/zlib/inflate.c
        src/3rdparty/zlib/inftrees.c
        src/3rdparty/zlib/trees.c
        src/3rdparty/zlib/uncompr.c
        src/3rdparty/zlib/zutil.c
        src/AlignedAllocator.cpp
        src/Allocator.cpp
        src/Barrier.cpp
        src/Base64.cpp
        src/BitSet.cpp
        src/BlockAllocator.cpp
        src/Buffer.cpp
        src/ChildProcess.cpp
        src/CommandLineOptions.cpp
        src/Condition.cpp
        src/Config.cpp
        src/Console.cpp
        src/ConsoleLogger.cpp
        src/CPU.cpp
        src/CRC32.cpp
        src/DefaultAllocator.cpp
        src/Directory.cpp
        src/DynamicCreatable.cpp
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
        src/JSON.cpp
        src/JobQueue.cpp
        src/JobQueuePool.cpp
        src/Logger.cpp
        src/LoggerMgr.cpp
        src/MD5.cpp
        src/MemoryLogger.cpp
        src/MimeTypeMapper.cpp
        src/Mutex.cpp
        src/NullAllocator.cpp
        src/NullLock.cpp
        src/NullLogger.cpp
        src/Path.cpp
        src/Pipeline.cpp
        src/PipelinePool.cpp
        src/Plugins.cpp
        src/Point.cpp
        src/RandomSeekSerializer.cpp
        src/RandomSource.cpp
        src/Rectangle.cpp
        src/RefCounted.cpp
        src/RunLoop.cpp
        src/RunLoopScheduler.cpp
        src/RWLock.cpp
        src/Scheduler.cpp
        src/SecureAllocator.cpp
        src/Semaphore.cpp
        src/Serializable.cpp
        src/SerializableHeader.cpp
        src/SerializableString.cpp
        src/SerializableValues.cpp
        src/Serializer.cpp
        src/SHA1.cpp
        src/SHA2.cpp
        src/SHA2_224_256.cpp
        src/SHA2_384_512.cpp
        src/SHA3.cpp
        src/SharedAllocator.cpp
        src/SharedObject.cpp
        src/SizeT.cpp
        src/SpinLock.cpp
        src/SpinRWLock.cpp
        src/StdAllocator.cpp
        src/StringUtils.cpp
        src/SystemInfo.cpp
        src/Thread.cpp
        src/ThreadRunLoop.cpp
        src/Timer.cpp
        src/TimeSpec.cpp
        src/TransactedFile.cpp
        src/TransactedFileAllocator.cpp
        src/TransactedFileBTree.cpp
        src/TransactedFileBTreeAllocator.cpp
        src/TransactedFileBTreeAllocatorBTree.cpp
        src/TransactedFileBTreeKeys.cpp
        src/TransactedFileBTreeRegistry.cpp
        src/TransactedFileObject.cpp
        src/TransactedFileRange.cpp
        src/TransactedFileRegistry.cpp
        src/ValueParser.cpp
        src/Variant.cpp
        src/Vectorizer.cpp
        src/Version.cpp
        src/XMLUtils.cpp
        src/os/osx/NSLogLogger.cpp
        src/3rdparty/pugixml/pugixml.cpp
        src/3rdparty/boost/atomic/lockpool.cpp
        src/os/osx/OSXUtils.mm
        examples/pathfinder/src/main.cpp
        examples/pathfinder/src/IgnoreList.cpp
        examples/pathfinder/src/Options.cpp
        examples/pathfinder/src/Root.cpp
        examples/pathfinder/src/Roots.cpp
        examples/pathfinder/src/Version.cpp)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        # where the library itself will look for its internal headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty/private
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty/private/zlib
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/pathfinder/include
    PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror
    -m64
    -Wall
    -Wno-deprecated-declarations
    -fPIC
    -fno-common
    -pipe
    -march=native
    -mno-avx
    -Wno-undefined-var-template)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FOUNDATION_FRAMEWORK}
    ${SECURITY_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${COCOA_FOUNDATION_FRAMEWORK}
    ${CORE_SERVICES_FRAMEWORK}
    ${CORE_GRAPHICS_FRAMEWORK}
    ${SYSTEM_CONFIGURATION_FRAMEWORK}
    ${IOKIT_FRAMEWORK})

# where to find our CMake modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#message(STATUS "CMake module path: ${CMAKE_MODULE_PATH}")
include(Installing)
