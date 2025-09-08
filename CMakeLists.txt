cmake_minimum_required(VERSION 3.22)
project(binance_demo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
# nlohmann_json doesn't have pkg-config, just include it directly

# Create the Binance futures demo executable
add_executable(binance_futures_demo
    demo/src/binance_futures_demo.cpp
    demo/src/binance_websocket_client.cpp
)

# Create the Gate.io futures demo executable
add_executable(gateio_futures_demo
    demo/src/gateio_futures_demo.cpp
    demo/src/gateio_websocket_client.cpp
)

# Set include directories for Binance demo
target_include_directories(binance_futures_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/demo/include
)

# Set include directories for Gate.io demo
target_include_directories(gateio_futures_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/demo/include
)

# Link libraries for Binance demo
target_link_libraries(binance_futures_demo PRIVATE 
    ${LIBCURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

# Link libraries for Gate.io demo
target_link_libraries(gateio_futures_demo PRIVATE 
    ${LIBCURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

# Set library include directories for Binance demo
target_include_directories(binance_futures_demo PRIVATE 
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Set library include directories for Gate.io demo
target_include_directories(gateio_futures_demo PRIVATE 
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Set compile options for Binance demo
target_compile_options(binance_futures_demo PRIVATE 
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBWEBSOCKETS_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
)

# Set compile options for Gate.io demo
target_compile_options(gateio_futures_demo PRIVATE 
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBWEBSOCKETS_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
)
