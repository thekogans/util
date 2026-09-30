cmake_minimum_required(VERSION 3.16...3.25)

# ==============================================================================
# 1. Versioning Details & Project Initialization
# ==============================================================================
set(THEKOGANS_UTIL_MAJOR_VERSION 0)
set(THEKOGANS_UTIL_MINOR_VERSION 39)
set(THEKOGANS_UTIL_PATCH_VERSION 0)

# Generate Hex Version String dynamically: (MAJOR << 16) | (MINOR << 8) | PATCH
math(EXPR VERSION_HEX "(${THEKOGANS_UTIL_MAJOR_VERSION} * 65536) + (${THEKOGANS_UTIL_MINOR_VERSION} * 256) + ${THEKOGANS_UTIL_PATCH_VERSION}")
string(MAKE_C_IDENTIFIER "${VERSION_HEX}" IS_VALID_HEX)
# Format accurately to a 8-digit hex value: 0x00002700
string(HEX ${VERSION_HEX} HEX_STRING)
string(LENGTH "${HEX_STRING}" HEX_LEN)
while(HEX_LEN LESS 8)
    set(HEX_STRING "0${HEX_STRING}")
    string(LENGTH "${HEX_STRING}" HEX_LEN)
endwhile()
set(THEKOGANS_UTIL_VERSION_HEX "0x${HEX_STRING}")

project(thekogans_util
    VERSION ${THEKOGANS_UTIL_MAJOR_VERSION}.${THEKOGANS_UTIL_MINOR_VERSION}.${THEKOGANS_UTIL_PATCH_VERSION}
    LANGUAGES C CXX
)

# ==============================================================================
# 2. Package & Feature Probing
# ==============================================================================
set(UTIL_FEATURES THEKOGANS_UTIL_TRANSACTED_FILE_ALLOCATOR_BLOCK_USE_MAGIC)

# Check for ZLIB
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    list(APPEND UTIL_FEATURES THEKOGANS_UTIL_HAVE_ZLIB)
endif()

# Platform-specific features and system dependency gathering
if(WIN32)
    list(APPEND UTIL_FEATURES THEKOGANS_UTIL_HAVE_OLE THEKOGANS_UTIL_HAVE_WTS)
    set(PLATFORM_LIBS Ws2_32 ole32 oleaut32 Wtsapi32 Advapi32)
elseif(LINUX OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND UTIL_FEATURES THEKOGANS_UTIL_HAVE_MMAP)

    # Check for X11 on Linux
    find_package(X11 QUIET)
    if(X11_FOUND)
        list(APPEND UTIL_FEATURES THEKOGANS_UTIL_HAVE_XLIB)
    endif()

    set(PLATFORM_LIBS rt dl)
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND UTIL_FEATURES THEKOGANS_UTIL_HAVE_MMAP)
    # Gather targeted macOS frameworks
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(CORE_SERVICES_FRAMEWORK CoreServices REQUIRED)
    find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
    find_library(SYSTEM_CONFIGURATION_FRAMEWORK SystemConfiguration REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)

    set(PLATFORM_LIBS
        ${FOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK} ${COCOA_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK} ${CORE_SERVICES_FRAMEWORK} ${CORE_GRAPHICS_FRAMEWORK}
        ${SYSTEM_CONFIGURATION_FRAMEWORK} ${IOKIT_FRAMEWORK}
    )
endif()

# ==============================================================================
# 3. Main Target Construction & Processor Definitions
# ==============================================================================
# Default to Shared libraries but respect global flags (-DBUILD_SHARED_LIBS=OFF)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" ON)
add_library(thekogans_util)

# 1. Define flat lists per compiler toolchain
set(GCC_NO_WARN "-Wno-unused-parameter" "-Wno-interference-size")
set(MSVC_NO_WARN      "/wd4100" "/wd4018")

# 2. ENFORCE QUOTES around the generator expressions so lists expand cleanly
target_compile_options(thekogans_util PUBLIC
    "$<$<CXX_COMPILER_ID:GNU>:${GCC_NO_WARN}>"
    "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_NO_WARN}>")

# Define explicit version flags
target_compile_definitions(thekogans_util PUBLIC
    THEKOGANS_UTIL_MAJOR_VERSION=${THEKOGANS_UTIL_MAJOR_VERSION}
    THEKOGANS_UTIL_MINOR_VERSION=${THEKOGANS_UTIL_MINOR_VERSION}
    THEKOGANS_UTIL_PATCH_VERSION=${THEKOGANS_UTIL_PATCH_VERSION}
    THEKOGANS_UTIL_VERSION=${THEKOGANS_UTIL_VERSION_HEX})

# Automated Configuration Context Definitions via Generator Expressions
# Maps THEKOGANS_UTIL_CONFIG_Release or THEKOGANS_UTIL_CONFIG_Debug dynamically based on active build target
target_compile_definitions(thekogans_util PUBLIC
    THEKOGANS_UTIL_CONFIG_$<CONFIG>)

# Maps THEKOGANS_UTIL_TYPE_Shared or THEKOGANS_UTIL_TYPE_Static automatically
target_compile_definitions(thekogans_util PUBLIC
    $<$<BOOL:${BUILD_SHARED_LIBS}>:THEKOGANS_UTIL_TYPE_Shared>
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:THEKOGANS_UTIL_TYPE_Static>
)

# Feature and platform baseline additions
target_compile_definitions(thekogans_util PUBLIC ${UTIL_FEATURES})

if(WIN32)
    target_compile_definitions(thekogans_util PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
        _LIB_THEKOGANS_UTIL_BUILD)
endif()

# Enforce C++17 transitively to downstream users
target_compile_features(thekogans_util PUBLIC cxx_std_17)

# Standardize include paths (Ensures 3rdparty/private remains purely internal)
target_include_directories(thekogans_util
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty/private
)

# Core Source Units
target_sources(thekogans_util PRIVATE
    src/AlignedAllocator.cpp src/Allocator.cpp src/Barrier.cpp src/Base64.cpp
    src/BitSet.cpp src/BlockAllocator.cpp src/Buffer.cpp src/ChildProcess.cpp
    src/CommandLineOptions.cpp src/Condition.cpp src/Config.cpp src/Console.cpp
    src/ConsoleLogger.cpp src/CPU.cpp src/CRC32.cpp src/DefaultAllocator.cpp
    src/Directory.cpp src/DynamicCreatable.cpp src/DynamicLibrary.cpp src/Event.cpp
    src/Exception.cpp src/File.cpp src/FileLogger.cpp src/Fraction.cpp src/GUID.cpp
    src/HRTimer.cpp src/HRTimerMgr.cpp src/Hash.cpp src/Heap.cpp src/JSON.cpp
    src/JobQueue.cpp src/JobQueuePool.cpp src/Logger.cpp src/LoggerMgr.cpp
    src/MD5.cpp src/MemoryLogger.cpp src/MimeTypeMapper.cpp src/Mutex.cpp
    src/NullAllocator.cpp src/NullLock.cpp src/NullLogger.cpp src/PageMap.cpp
    src/Path.cpp src/Pipeline.cpp src/PipelinePool.cpp src/Plugins.cpp src/Point.cpp
    src/RandomSeekSerializer.cpp src/RandomSource.cpp src/Rectangle.cpp
    src/RefCounted.cpp src/RunLoop.cpp src/RunLoopScheduler.cpp src/RWLock.cpp
    src/Scheduler.cpp src/SecureAllocator.cpp src/Semaphore.cpp src/Serializable.cpp
    src/SerializableHeader.cpp src/SerializableString.cpp src/SerializableValues.cpp
    src/Serializer.cpp src/SHA1.cpp src/SHA2.cpp src/SHA2_224_256.cpp
    src/SHA2_384_512.cpp src/SHA3.cpp src/SharedAllocator.cpp src/SharedObject.cpp
    src/SizeT.cpp src/SlabAllocatorDetail.cpp src/SpinLock.cpp src/SpinRWLock.cpp
    src/StdAllocator.cpp src/StringUtils.cpp src/SystemInfo.cpp src/Thread.cpp
    src/ThreadRunLoop.cpp src/Timer.cpp src/TimeSpec.cpp src/TransactedFile.cpp
    src/TransactedFileAllocator.cpp src/TransactedFileBTree.cpp
    src/TransactedFileBTreeAllocator.cpp src/TransactedFileBTreeAllocatorBTree.cpp
    src/TransactedFileBTreeKeys.cpp src/TransactedFileBTreeRegistry.cpp
    src/TransactedFileObject.cpp src/TransactedFileRange.cpp
    src/TransactedFileRegistry.cpp src/ValueParser.cpp src/Variant.cpp
    src/Vectorizer.cpp src/Version.cpp src/XMLUtils.cpp

    # Integrated 3rdparty code
    src/3rdparty/pugixml/pugixml.cpp
    src/3rdparty/boost/atomic/lockpool.cpp
)

# Platform Conditional Sources
if(WIN32)
    target_sources(thekogans_util PRIVATE
        src/os/windows/HGLOBALAllocator.cpp
        src/os/windows/HeapAllocator.cpp
        src/os/windows/OutputDebugStringLogger.cpp
        src/os/windows/WindowsFirewall.cpp
        src/os/windows/WindowsUtils.cpp
    )
elseif(APPLE)
    target_sources(thekogans_util PRIVATE
        src/os/osx/NSLogLogger.cpp
        src/os/osx/OSXUtils.mm
    )
else() # Linux branch
    if(X11_FOUND)
        target_sources(thekogans_util PRIVATE src/os/linux/XlibUtils.cpp)
    endif()
endif()

# ==============================================================================
# 4. Dependency Binding
# ==============================================================================
if(ZLIB_FOUND)
    target_link_libraries(thekogans_util PRIVATE ZLIB::ZLIB)
endif()

if(X11_FOUND AND (LINUX OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    target_link_libraries(thekogans_util PRIVATE X11::X11)
endif()

target_link_libraries(thekogans_util PUBLIC ${PLATFORM_LIBS})

# ==============================================================================
# 5. Embedded Assets
# ==============================================================================
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/mime_types.txt
    ${CMAKE_CURRENT_BINARY_DIR}/resources/mime_types.txt
    COPYONLY
)

# ==============================================================================
# 6. Test Suite Generation (Isolated Standalone Runners)
# ==============================================================================
option(BUILD_TESTING "Build the CppUnitXLite test suite" ON)

if(BUILD_TESTING)
    # 1. Define a clean list of your individual test source file names
    set(TEST_FILES
        test_RefCounted
        test_SharedAllocator
        test_SpinRWLock
        test_RWLock
        test_JobQueue
        test_Scheduler
        test_SlabAllocator
        test_Version
    )

    # 2. Loop through and instantiate an independent binary target for each file
    foreach(TEST_NAME IN LISTS TEST_FILES)

        # Creates a standalone executable target (e.g., test_Version_runner)
        add_executable(${TEST_NAME}_runner
            tests/${TEST_NAME}.cpp
        )

        # Link the core library (implicitly grabs include directories & C++17 flags)
        target_link_libraries(${TEST_NAME}_runner PRIVATE thekogans_util)

        # Point to the 3rdparty folder so #include <CppUnitXLite/CppUnitXLite.hpp> resolves
        target_include_directories(${TEST_NAME}_runner PRIVATE include/3rdparty)
    endforeach()
endif()

# ==============================================================================
# 7. Installation and Export Architecture
# ==============================================================================
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(UTIL_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/thekogans_util")

install(TARGETS thekogans_util
    EXPORT thekogans_util_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT thekogans_util_targets
    FILE thekogans_utilTargets.cmake
    NAMESPACE thekogans::
    DESTINATION ${UTIL_CMAKE_INSTALL_DIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    PATTERN "3rdparty" EXCLUDE
)

install(DIRECTORY include/3rdparty/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.cpp"
    PATTERN "private" EXCLUDE
)

install(FILES resources/mime_types.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/thekogans_util
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/thekogans_utilConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/thekogans_utilConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/thekogans_utilConfig.cmake"
    INSTALL_DESTINATION ${UTIL_CMAKE_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/thekogans_utilConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/thekogans_utilConfigVersion.cmake"
    DESTINATION ${UTIL_CMAKE_INSTALL_DIR}
)

if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in" # Updated path
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
endif()

# ==============================================================================
# 8. Automated Pkg-Config Generation (.pc File)
# ==============================================================================
set(PC_LIBS_PRIVATE "")
foreach(LIB_ITEM IN LISTS PLATFORM_LIBS)
    set(PC_LIBS_PRIVATE "${PC_LIBS_PRIVATE} -l${LIB_ITEM}")
endforeach()

if(ZLIB_FOUND)
    set(PC_REQUIRES_PRIVATE "zlib")
endif()

# Loop through features list and format them with -D
set(PC_UTIL_FEATURES "")
foreach(feature IN LISTS UTIL_FEATURES)
    string(APPEND PC_UTIL_FEATURES " -D${feature}")
endforeach()

# Generate the .pc file using configure_file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/thekogans_util.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/thekogans_util.pc"
    @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/thekogans_util.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
