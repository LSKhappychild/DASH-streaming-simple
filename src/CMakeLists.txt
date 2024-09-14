cmake_minimum_required(VERSION 3.10)
set(CMAKE_BUILD_TYPE Debug)

# Define the project name
project(MyProject)
find_package(Threads REQUIRED)
find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(OPENSSL_ROOT_DIR /usr/local/ssl)

# Add the server executable
add_executable(my_server ../src/server.cpp)

# Include directories for CivetWeb and libdash
include_directories(../library/civetweb/include)
include_directories(../library/libdash/libdash/libdash/include)  
include_directories(../library/libdash/libdash/libdash/source)  

# Add CivetWeb source file to server executable
target_sources(my_server PRIVATE ../library/civetweb/src/civetweb.c)

# Define OpenSSL usage
add_definitions(-DOPENSSL_API_1_1)

# Find and link OpenSSL
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Link OpenSSL, pthreads, and dl to the server
target_link_libraries(my_server PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads dl)

# Specify the path to libdash and ensure it's found correctly
set(LIBDASH_INCLUDE_DIR "../library/libdash/libdash/libdash/include")  # Adjust path if needed
set(LIBDASH_LIBRARY "/home/sklee/DASH-streaming-simple/library/libdash/libdash/build/bin/libdash.so")  # Path to libdash.so

# Include directories for client
include_directories(${LIBDASH_INCLUDE_DIR})

# Add the client executable
add_executable(my_client ../src/client.cpp)

# Link libdash and other libraries to the client executable
target_link_libraries(my_client PRIVATE ${LIBDASH_LIBRARY})

# Link pthreads to the client executable
target_link_libraries(my_client PRIVATE Threads::Threads CURL::libcurl)

# Ensure CMake knows where to look for the libdash library during runtime
set_target_properties(my_client PROPERTIES LINK_FLAGS "-Wl,-rpath,${LIBDASH_LIBRARY}")
