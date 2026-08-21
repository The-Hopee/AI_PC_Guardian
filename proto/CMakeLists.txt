include(FetchContent)

set(GUARDIAN_PROTOBUF_TAG "v35.0")

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBUPB ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

# EXCLUDE_FROM_ALL появился в CMake 3.28. На более старых версиях
# сохраняем совместимость с минимальной версией проекта — CMake 3.25.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG "${GUARDIAN_PROTOBUF_TAG}"
        GIT_SHALLOW TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
else()
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG "${GUARDIAN_PROTOBUF_TAG}"
        GIT_SHALLOW TRUE
        SYSTEM
    )
endif()

FetchContent_MakeAvailable(protobuf)

message(STATUS "Using Protobuf from tag ${GUARDIAN_PROTOBUF_TAG}")

include("${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake")

set(GUARDIAN_TELEMETRY_PROTO
    "${CMAKE_CURRENT_SOURCE_DIR}/guardian/v1/telemetry.proto"
)
set(GUARDIAN_PROTO_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

file(MAKE_DIRECTORY "${GUARDIAN_PROTO_GENERATED_DIR}")

protobuf_generate(
    OUT_VAR GUARDIAN_PROTO_GENERATED_SOURCES
    LANGUAGE cpp
    PROTOS "${GUARDIAN_TELEMETRY_PROTO}"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOC_OUT_DIR "${GUARDIAN_PROTO_GENERATED_DIR}"
)

add_library(
    guardian_proto_messages STATIC
    ${GUARDIAN_PROTO_GENERATED_SOURCES}
)

target_link_libraries(
    guardian_proto_messages
    PUBLIC protobuf::libprotobuf
)

target_include_directories(
    guardian_proto_messages
    SYSTEM
    PUBLIC
        "$<BUILD_INTERFACE:${GUARDIAN_PROTO_GENERATED_DIR}>"
)

add_library(
    guardian_proto STATIC
    src/telemetry_conversion.cpp
)
add_library(Guardian::Proto ALIAS guardian_proto)

target_link_libraries(
    guardian_proto
    PUBLIC
        Guardian::Common
        guardian_proto_messages
)

target_include_directories(
    guardian_proto
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
