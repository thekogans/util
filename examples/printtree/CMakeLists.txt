cmake_minimum_required(VERSION 3.16...3.25)

# ==============================================================================
# 1. Project Initialization & Program Metadata
# ==============================================================================
set(THEKOGANS_PRINTTREE_MAJOR_VERSION 0)
set(THEKOGANS_PRINTTREE_MINOR_VERSION 2)
set(THEKOGANS_PRINTTREE_PATCH_VERSION 0)

project(thekogans_printtree
    VERSION ${THEKOGANS_PRINTTREE_MAJOR_VERSION}.${THEKOGANS_PRINTTREE_MINOR_VERSION}.${THEKOGANS_PRINTTREE_PATCH_VERSION}
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
add_executable(thekogans_printtree)

# Track program-specific versions if your internal source files depend on them
target_compile_definitions(thekogans_printtree PRIVATE
    THEKOGANS_PRINTTREE_MAJOR_VERSION=${THEKOGANS_PRINTTREE_MAJOR_VERSION}
    THEKOGANS_PRINTTREE_MINOR_VERSION=${THEKOGANS_PRINTTREE_MINOR_VERSION}
    THEKOGANS_PRINTTREE_PATCH_VERSION=${THEKOGANS_PRINTTREE_PATCH_VERSION}
)

# Set up local headers search scope mirroring private = "yes"
target_include_directories(thekogans_printtree PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Program Source Units
target_sources(thekogans_printtree PRIVATE
    src/main.cpp
)

# ==============================================================================
# 4. Dependency & System Subsystem Binding
# ==============================================================================
# Automatically injects include paths, target features, and linkage properties
target_link_libraries(thekogans_printtree PRIVATE thekogans::thekogans_util)

# Recreates <subsystem>Console</subsystem> specifically for Windows compiler drivers
if(WIN32)
    set_target_properties(thekogans_printtree PROPERTIES
        WIN32_EXECUTABLE FALSE # Sets subsystem explicitly to CONSOLE instead of WINDOWS
    )
endif()
