cmake_minimum_required(VERSION 3.1.0)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/des_crypt.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/mac_addr.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/nu_console.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/nu_logger.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/nu_terminal.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/udp_socket.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/vndd_mgr.cc
      ${CMAKE_CURRENT_SOURCE_DIR}/vndd_setup.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/vndd_tunnel.cc 
      ${CMAKE_CURRENT_SOURCE_DIR}/vndd_vpnd.cc
)

add_executable(vpnd ${SOURCES})
target_link_libraries(vpnd PRIVATE Threads::Threads)

