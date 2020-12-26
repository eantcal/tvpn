cmake_minimum_required(VERSION 3.1.0)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/vpnd)
file(GLOB SOURCES 
	${CMAKE_CURRENT_SOURCE_DIR}/vndd_config.cc
	${PROJECT_SOURCE_DIR}/vpnd/mac_addr.cc
	${PROJECT_SOURCE_DIR}/vpnd/vndd_mgr.cc
)
add_executable(vpncfg ${SOURCES})
