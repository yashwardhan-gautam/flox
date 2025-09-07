add_executable(flox_demo
    src/main.cpp
    src/demo_connector.cpp
    src/demo_strategy.cpp
    src/demo_builder.cpp
)

target_include_directories(flox_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                            ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(flox_demo PRIVATE ${FLOX})

# Binance Futures REST API Demo
add_executable(binance_futures_demo
    src/binance_futures_demo.cpp
    src/binance_rest_client.cpp
)

target_include_directories(binance_futures_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                                        ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

target_link_libraries(binance_futures_demo PRIVATE 
    ${FLOX}
    ${LIBCURL_LIBRARIES}
)

target_include_directories(binance_futures_demo PRIVATE ${LIBCURL_INCLUDE_DIRS})
target_compile_options(binance_futures_demo PRIVATE ${LIBCURL_CFLAGS_OTHER})

# Binance WebSocket + REST Order Book Demo
add_executable(binance_websocket_demo
    src/binance_websocket_demo.cpp
    src/binance_rest_client.cpp
    src/binance_websocket_client.cpp
    src/local_order_book.cpp
)

target_include_directories(binance_websocket_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(binance_websocket_demo PRIVATE 
    ${FLOX}
    ${LIBCURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
)

target_include_directories(binance_websocket_demo PRIVATE 
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

target_compile_options(binance_websocket_demo PRIVATE 
    ${LIBCURL_CFLAGS_OTHER}
    ${LIBWEBSOCKETS_CFLAGS_OTHER}
)
