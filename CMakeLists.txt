cmake_minimum_required(VERSION 3.20)
project(pqc_orange_pi C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)   # we use POSIX (clock_gettime, getaddrinfo, poll)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-O2 -Wall -Wextra)

# --- Architecture selection ------------------------------------------------
# PQ_ARCH is x86_64 by default; the armhf toolchain file sets it to armhf.
if(NOT DEFINED PQ_ARCH)
  set(PQ_ARCH "x86_64")
endif()
message(STATUS "PQ_ARCH = ${PQ_ARCH}")

# --- Pinned OpenSSL 3.5.9 (static) -----------------------------------------
set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/install/${PQ_ARCH}/openssl")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
find_package(OpenSSL 3.5.9 REQUIRED)
message(STATUS "OpenSSL: ${OPENSSL_VERSION} (${OPENSSL_SSL_LIBRARY})")

find_package(Threads REQUIRED)

# Fully static binaries so armhf artifacts run on the Pi regardless of its
# glibc, and so the pinned OpenSSL is provably the only one linked.
add_link_options(-static)

# --- libcommon -------------------------------------------------------------
add_library(common STATIC
  src/common/net.c
  src/common/timing.c
  src/common/csv.c
  src/common/version.c
  src/common/tlsctx.c
  src/common/confirm.c)
target_include_directories(common PUBLIC src/common)
target_link_libraries(common PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})

# --- executables -----------------------------------------------------------
add_executable(kem_bench src/kem_bench/main.c)
target_link_libraries(kem_bench PRIVATE common)

add_executable(pq_node src/pq_node/main.c src/pq_node/args.c)
target_link_libraries(pq_node PRIVATE common)

# --- tests -----------------------------------------------------------------
enable_testing()
add_executable(test_common tests/test_common.c)
target_link_libraries(test_common PRIVATE common)
add_test(NAME test_common COMMAND test_common)

add_executable(test_kem tests/test_kem.c)
target_link_libraries(test_kem PRIVATE common)
add_test(NAME test_kem COMMAND test_kem)

add_executable(test_confirm tests/test_confirm.c)
target_link_libraries(test_confirm PRIVATE common)
add_test(NAME test_confirm COMMAND test_confirm)
