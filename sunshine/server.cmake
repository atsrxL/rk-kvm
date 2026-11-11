# Dedicated host; Sunshine's CMake files are never evaluated.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RKMOON_SUNSHINE_SOURCE "${PROJECT_SOURCE_DIR}/vendor/sunshine" CACHE PATH "Clean pinned Sunshine checkout")
set(RKMOON_BOOST_SOURCE "" CACHE PATH "Optional existing Boost source tree (offline builds)")
set(RKMOON_PROTOCOL_ROOT "${CMAKE_CURRENT_BINARY_DIR}/protocol")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/prepare_sunshine.py"
  "${RKMOON_SUNSHINE_SOURCE}" --output "${RKMOON_PROTOCOL_ROOT}" --refresh COMMAND_ERROR_IS_FATAL ANY)
if(RKMOON_BOOST_SOURCE)
  set(BOOST_INCLUDE_LIBRARIES filesystem log process system thread endian format property_tree)
  add_subdirectory("${RKMOON_BOOST_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/boost" EXCLUDE_FROM_ALL)
else()
  find_package(Boost 1.87 REQUIRED COMPONENTS filesystem log thread process endian format property_tree)
endif()
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED IMPORTED_TARGET opus)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
set(P "${RKMOON_PROTOCOL_ROOT}")
add_library(rkmoon_enet STATIC
 ${P}/third-party/moonlight-common-c/enet/callbacks.c
 ${P}/third-party/moonlight-common-c/enet/compress.c
 ${P}/third-party/moonlight-common-c/enet/host.c
 ${P}/third-party/moonlight-common-c/enet/list.c
 ${P}/third-party/moonlight-common-c/enet/packet.c
 ${P}/third-party/moonlight-common-c/enet/peer.c
 ${P}/third-party/moonlight-common-c/enet/protocol.c
 ${P}/third-party/moonlight-common-c/enet/unix.c)
target_include_directories(rkmoon_enet PUBLIC ${P}/third-party/moonlight-common-c/enet/include)
target_compile_definitions(rkmoon_enet PRIVATE HAS_FCNTL=1 HAS_IOCTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1 HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
add_executable(rkmoon-kvm
 ${P}/src/stream.cpp ${P}/src/rtsp.cpp ${P}/src/input.cpp ${P}/src/crypto.cpp
 ${P}/src/network.cpp ${P}/src/logging.cpp ${P}/src/globals.cpp
 ${P}/src/rkmoon/rkmoon_platform.cpp ${P}/src/rkmoon/rkmoon_input.cpp
 ${P}/src/rkmoon/rkmoon_bridge.cpp ${P}/src/rkmoon/rkmoon_audio.cpp ${P}/src/rkmoon/rkmoon_main.cpp
 ${P}/src/rkmoon/rkmoon_http.cpp ${P}/src/rkmoon/rkmoon_config.cpp ${P}/src/rkmoon/rkmoon_modules.cpp
 ${P}/third-party/moonlight-common-c/src/RtspParser.c
 ${P}/third-party/moonlight-common-c/nanors/rs.c
 ${P}/third-party/moonlight-common-c/nanors/deps/obl/oblas_common.c
 ${P}/third-party/moonlight-common-c/nanors/deps/obl/oblas_lite.c)
target_compile_features(rkmoon-kvm PRIVATE cxx_std_23)
target_include_directories(rkmoon-kvm PRIVATE ${P} ${P}/src ${PROJECT_SOURCE_DIR}/include
 ${P}/third-party ${P}/third-party/Simple-Web-Server
 ${P}/third-party/moonlight-common-c/nanors ${P}/third-party/moonlight-common-c/nanors/deps/obl)
target_compile_definitions(rkmoon-kvm PRIVATE SUNSHINE_PLATFORM="linux" SUNSHINE_TRAY=0)
target_link_libraries(rkmoon-kvm PRIVATE rkmoon_core rkmoon_enet OpenSSL::SSL OpenSSL::Crypto
 Boost::filesystem Boost::log Boost::thread Boost::process Boost::endian Boost::format Boost::property_tree PkgConfig::OPUS PkgConfig::ALSA)
file(COPY "${P}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
