cmake_minimum_required(VERSION 3.16...3.25)

project(thekogans_examples LANGUAGES C CXX)

# ==============================================================================
# 1. Unified Dependency Verification
# ==============================================================================
# Ensure the host machine has the baseline core library deployed before continuing
find_package(thekogans_util 0.39 REQUIRED)

# ==============================================================================
# 2. Sequential Example Project Registration (Isolated Scopes)
# ==============================================================================
# Register all util-dependent example programs here
add_subdirectory(genguid)
add_subdirectory(hash)
add_subdirectory(pathfinder)
add_subdirectory(printtree)
add_subdirectory(systeminfo)
