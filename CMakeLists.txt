cmake_minimum_required(VERSION 3.16)
project(KenshiMultiplayer LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

# MSVC: /utf-8 for source encoding
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================
# Third-party dependencies
# ============================================

# miniupnp for UPnP port mapping (static linking)
set(UPNPC_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(UPNPC_BUILD_TESTS OFF CACHE BOOL "Build test executables" FORCE)
set(UPNPC_BUILD_SAMPLE OFF CACHE BOOL "Build sample executables" FORCE)
set(UPNPC_NO_INSTALL ON CACHE BOOL "Disable installation" FORCE)

add_subdirectory(third_party/miniupnp/miniupnpc EXCLUDE_FROM_ALL)

# Launcher executable
add_executable(KenshiMultiplayer WIN32
    src/Launcher/main.cpp
    src/Launcher/injector.cpp
    src/Launcher/injector.h
    src/Launcher/config.cpp
    src/Launcher/config.h
    src/Launcher/local_ip.cpp
    src/Launcher/local_ip.h
    src/Launcher/launcher_ui.cpp
    src/Launcher/launcher_ui.h
)

target_link_libraries(KenshiMultiplayer PRIVATE
    psapi
    iphlpapi
    ws2_32
)
if(MSVC)
    set_source_files_properties(src/Launcher/main.cpp PROPERTIES COMPILE_FLAGS "/EHa")
endif()

# Core DLL (injected into game)
add_library(KenshiMP_Core SHARED
    src/Core/dllmain.cpp
    src/Core/memory.cpp
    src/Core/memory.h
    src/Core/logger.cpp
    src/Core/logger.h
    src/Core/version.h
    src/Core/game_types.cpp
    src/Core/game_types.h
    src/Core/game_memory.cpp
    src/Core/game_memory.h
    src/Core/game_hooks.cpp
    src/Core/game_hooks.h
    src/Core/network.cpp
    src/Core/network.h
    src/Core/protocol.cpp
    src/Core/protocol.h
    src/Core/sync_manager.cpp
    src/Core/sync_manager.h
    src/Core/mp_config.cpp
    src/Core/mp_config.h
    src/Core/game_loop.cpp
    src/Core/game_loop.h
    src/Core/mod_check.cpp
    src/Core/mod_check.h
    src/Core/exception_handler.cpp
    src/Core/exception_handler.h

    # NAT Traversal Module
    src/Core/nat_traversal.cpp
    src/Core/nat_traversal.h
    src/Core/upnp_manager.cpp
    src/Core/upnp_manager.h
    src/Core/stun_manager.cpp
    src/Core/stun_manager.h
    src/Core/local_ip.cpp
    src/Core/local_ip.h
)

target_link_libraries(KenshiMP_Core PRIVATE
    psapi
    ws2_32
    iphlpapi
    libminiupnpc-static
)

target_include_directories(KenshiMP_Core PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core
    ${CMAKE_SOURCE_DIR}/third_party/miniupnp/miniupnpc/include
)
if(MSVC)
    set_source_files_properties(src/Core/game_hooks.cpp PROPERTIES COMPILE_FLAGS "/EHa")
endif()

# Protocol unit test (no game dependency)
add_executable(KenshiMP_ProtocolTest
    tests/protocol_test.cpp
    src/Core/protocol.cpp
    src/Core/game_types.cpp
)
target_include_directories(KenshiMP_ProtocolTest PRIVATE ${CMAKE_SOURCE_DIR}/src/Core)
add_custom_command(TARGET KenshiMP_ProtocolTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:KenshiMP_ProtocolTest> ${CMAKE_SOURCE_DIR}/
)

# Copy output to root
add_custom_command(TARGET KenshiMultiplayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:KenshiMultiplayer> ${CMAKE_SOURCE_DIR}/
)
add_custom_command(TARGET KenshiMP_Core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:KenshiMP_Core> ${CMAKE_SOURCE_DIR}/
)
