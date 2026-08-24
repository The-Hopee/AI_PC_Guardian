include(FetchContent)

set(GUARDIAN_GRPC_TAG "v1.83.0")
set(GUARDIAN_PROTOBUF_TAG "v35.1")

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBUPB ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_CODEGEN ON CACHE BOOL "" FORCE)
set(gRPC_DOWNLOAD_ARCHIVES OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPCPP_OTEL_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)

# EXCLUDE_FROM_ALL появился в CMake 3.28. На более старых версиях
# сохраняем совместимость с минимальной версией проекта — CMake 3.25.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG "${GUARDIAN_GRPC_TAG}"
        GIT_SHALLOW TRUE
        GIT_SUBMODULES
            third_party/abseil-cpp
            third_party/boringssl-with-bazel
            third_party/cares/cares
            third_party/protobuf
            third_party/re2
            third_party/zlib
        GIT_SUBMODULES_RECURSE TRUE
        SYSTEM
        EXCLUDE_FROM_ALL
    )
else()
    FetchContent_Declare(
        grpc
        GIT_REPOSITORY https://github.com/grpc/grpc.git
        GIT_TAG "${GUARDIAN_GRPC_TAG}"
        GIT_SHALLOW TRUE
        GIT_SUBMODULES
            third_party/abseil-cpp
            third_party/boringssl-with-bazel
            third_party/cares/cares
            third_party/protobuf
            third_party/re2
            third_party/zlib
        GIT_SUBMODULES_RECURSE TRUE
        SYSTEM
    )
endif()

FetchContent_MakeAvailable(grpc)

message(
    STATUS
    "Using gRPC ${GUARDIAN_GRPC_TAG} with bundled Protobuf ${GUARDIAN_PROTOBUF_TAG}"
)

include(
    "${grpc_SOURCE_DIR}/third_party/protobuf/cmake/protobuf-generate.cmake"
)

set(GUARDIAN_TELEMETRY_PROTO
    "${CMAKE_CURRENT_SOURCE_DIR}/guardian/v1/telemetry.proto"
)
set(GUARDIAN_TELEMETRY_SERVICE_PROTO
    "${CMAKE_CURRENT_SOURCE_DIR}/guardian/v1/telemetry_service.proto"
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

protobuf_generate(
    OUT_VAR GUARDIAN_RPC_PROTO_GENERATED_SOURCES
    LANGUAGE cpp
    PROTOS "${GUARDIAN_TELEMETRY_SERVICE_PROTO}"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOC_OUT_DIR "${GUARDIAN_PROTO_GENERATED_DIR}"
)

protobuf_generate(
    OUT_VAR GUARDIAN_RPC_GRPC_GENERATED_SOURCES
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>"
    DEPENDENCIES grpc_cpp_plugin
    PROTOS "${GUARDIAN_TELEMETRY_SERVICE_PROTO}"
    IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    PROTOC_OUT_DIR "${GUARDIAN_PROTO_GENERATED_DIR}"
)

add_library(
    guardian_rpc STATIC
    ${GUARDIAN_RPC_PROTO_GENERATED_SOURCES}
    ${GUARDIAN_RPC_GRPC_GENERATED_SOURCES}
)
add_library(Guardian::Rpc ALIAS guardian_rpc)

target_link_libraries(
    guardian_rpc
    PUBLIC
        guardian_proto_messages
        grpc++
)

target_include_directories(
    guardian_rpc
    SYSTEM
    PUBLIC
        "$<BUILD_INTERFACE:${GUARDIAN_PROTO_GENERATED_DIR}>"
)
