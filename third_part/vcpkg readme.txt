安装vckpg
git clone https://github.com/microsoft/vcpkg
1.运行bootstrap-vcpkg.bat下载vcpkg.exe
2.运行验证 ./vcpkg
3.安装 openssl ./vcpkg install openssl
4.安装lua ./vcpkg install lua

vcpkg install curl:x64-windows
vcpkg install libxml2:x64-windows
vcpkg install log4cxx:x64-windows

vcpkg install lua:x64-windows

vcpkg install boost:x64-windows
vcpkg install openssl:x64-windows

vcpkg install hiredis:x64-windows
vcpkg install pbc:x64-windows

vcpkg install protobuf-c:x64-windows
vcpkg install protobuf:x64-windows
