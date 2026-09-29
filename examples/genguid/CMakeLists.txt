cmake_minimum_required(VERSION 3.16...3.25)

# ==============================================================================
# 1. Project Initialization & Program Metadata
# ==============================================================================
set(THEKOGANS_GENGUID_MAJOR_VERSION 0)
set(THEKOGANS_GENGUID_MINOR_VERSION 1)
set(THEKOGANS_GENGUID_PATCH_VERSION 0)

project(thekogans_genguid
    VERSION ${THEKOGANS_GENGUID_MAJOR_VERSION}.${THEKOGANS_GENGUID_MINOR_VERSION}.${THEKOGANS_GENGUID_PATCH_VERSION}
    LANGUAGES C CXX
)

# ==============================================================================
# 2. Dependency Tracking (Locates Your Installed Library Natively)
# ==============================================================================
# Binds to the system-installed metadata generated during your util build
find_package(thekogans_util 0.39 REQUIRED)

# ==============================================================================
# 3. Main Program Target Construction
# ==============================================================================
add_executable(thekogans_genguid)

# Track program-specific versions if your internal source files depend on them
target_compile_definitions(thekogans_genguid PRIVATE
    THEKOGANS_GENGUID_MAJOR_VERSION=${THEKOGANS_GENGUID_MAJOR_VERSION}
    THEKOGANS_GENGUID_MINOR_VERSION=${THEKOGANS_GENGUID_MINOR_VERSION}
    THEKOGANS_GENGUID_PATCH_VERSION=${THEKOGANS_GENGUID_PATCH_VERSION}
)

# Set up local headers search scope mirroring private = "yes"
target_include_directories(thekogans_genguid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Program Source Units
target_sources(thekogans_genguid PRIVATE
    src/main.cpp
)

# ==============================================================================
# 4. Dependency & System Subsystem Binding
# ==============================================================================
# Automatically injects include paths, target features, and linkage properties
target_link_libraries(thekogans_genguid PRIVATE thekogans::thekogans_util)

# Recreates <subsystem>Console</subsystem> specifically for Windows compiler drivers
if(WIN32)
    set_target_properties(thekogans_genguid PROPERTIES
        WIN32_EXECUTABLE FALSE # Sets subsystem explicitly to CONSOLE instead of WINDOWS
    )
endif()
