cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0167 NEW)
cmake_policy(SET CMP0074 NEW)

project(chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/opt/homebrew/opt/protobuf@3")

find_package(Boost REQUIRED COMPONENTS thread)

set(Protobuf_ROOT "/opt/homebrew/opt/protobuf@3")
set(Protobuf_PROTOC_EXECUTABLE "${Protobuf_ROOT}/bin/protoc")
set(Protobuf_INCLUDE_DIR "${Protobuf_ROOT}/include")
set(Protobuf_LIBRARY "${Protobuf_ROOT}/lib/libprotobuf.a")
find_package(Protobuf REQUIRED)

find_library(SQLITE3_LIB sqlite3 PATHS /opt/homebrew/lib /usr/local/lib REQUIRED)
find_library(SODIUM_LIB sodium PATHS /opt/homebrew/lib /usr/local/lib REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/chat.proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_SRCS ${PROTO_GEN_DIR}/chat.pb.cc)
set(PROTO_HDRS ${PROTO_GEN_DIR}/chat.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --cpp_out=${PROTO_GEN_DIR}
        -I${CMAKE_CURRENT_SOURCE_DIR}/proto
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf C++ from chat.proto"
)

set(CHAT_SERVER_SRCS
    src/main.cpp
    src/Session.cpp
    src/ChatServer.cpp
    src/ChatService.cpp
    src/Codec.cpp
    src/OnlineRegistry.cpp
    src/store/UserStore.cpp
    src/store/FriendStore.cpp
    src/store/MessageStore.cpp
    src/auth/PasswordHash.cpp
    ${PROTO_SRCS}
)

function(setup_chat_target target)
    target_compile_definitions(${target} PRIVATE NDEBUG)
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROTO_GEN_DIR}
    )
    target_include_directories(${target} BEFORE PRIVATE "${Protobuf_ROOT}/include")
    find_library(Z_LIB z PATHS /opt/homebrew/lib /usr/lib REQUIRED)
    target_link_libraries(${target} PRIVATE
        Boost::thread
        ${SQLITE3_LIB}
        ${SODIUM_LIB}
        ${Protobuf_LIBRARY}
        ${Z_LIB}
    )
endfunction()

add_executable(chat ${CHAT_SERVER_SRCS})
setup_chat_target(chat)

add_executable(chat_cli test/chat_cli.cpp src/Codec.cpp ${PROTO_SRCS})
setup_chat_target(chat_cli)

add_executable(proto_client test/proto_client.cpp src/Codec.cpp ${PROTO_SRCS})
setup_chat_target(proto_client)
