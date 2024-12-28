:: RPC 协议编译
protoc --cpp_out=../proto-cpp/protocol --grpc_out=../proto-cpp/protocol --plugin=protoc-gen-grpc=D:/github/vcpkg/packages/grpc_x64-windows/tools/grpc/grpc_cpp_plugin.exe rpc/myservice.proto


:: MSG 协议编译
protoc --cpp_out=../proto-cpp/protocol msg/login.proto

