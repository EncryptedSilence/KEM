# Cross-compile for the Orange Pi R1 (32-bit ARMv7, armhf).
# Produces fully static binaries (see add_link_options(-static) in CMakeLists),
# so the Pi's glibc version does not matter.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(PQ_ARCH "armhf" CACHE STRING "target arch" FORCE)

# Compiler prefix is configurable (Bootlin uses arm-buildroot-linux-gnueabihf-,
# Debian/Ubuntu use arm-linux-gnueabihf-). Set PQ_CROSS_PREFIX in the env.
if(DEFINED ENV{PQ_CROSS_PREFIX})
  set(_pfx "$ENV{PQ_CROSS_PREFIX}")
else()
  set(_pfx "arm-linux-gnueabihf-")
endif()
set(CMAKE_C_COMPILER   "${_pfx}gcc")
set(CMAKE_CXX_COMPILER "${_pfx}g++")

# Find the pinned armhf OpenSSL under the project, not the host.
get_filename_component(_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH "${_repo}/install/armhf/openssl")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
