


lib - engine 生成lib以及include
data - excel 配置文件数据 以及 proto 文件
document - 文档目录
third_part - 第三方库目录 直接使用 vcpkg 进行安装
tools - 工具源码目录
build - cmake 生成项目文件目录
core - 封装进程核心
service - 封装service基础接口






RealServer/
├── CMakeLists.txt                   # 主 CMake 配置文件
├── src/                             
│   ├── engine/                      # engine 项目 提供给service依赖的库
│   |       ├── xAsio
│   |       ├── xBase
│   |       └── CMakeLists.txt
|   └── service/                      
│           ├── login/                # LoginService 实例 依赖 engine 项目 以及依赖 source 源码目录 
│           |     ├── src/
│           |     |    |── EventHandler/
│           |     |    └── ... .. .
│           |     └── CMakeLists.txt
│           └── gate/                  # GateService 实例 依赖 engine 项目 以及依赖 source 源码目录 
│                 ├── src/
│                 |    |── EventHandler/
│                 |    └── ... .. .
│                 └── CMakeLists.txt
├── source/
│   │ 
│   ├── proto-cpp 
│   │       ├── excel                  # excel 配置目录
│   │       ├── config                 # excel 配置源码生成目录
│   │       └── protocol 
│   │                ├── msg           # msg协议生成目录
│   │                └── rpc           # rpc协议生成目录
│   └── proto-def 
│           ├── msg                    # msg结构定义目录
│           └── rpc                    # rpc结构定义目录
├── lib/               
├── third_part/               
├── document/               
└── build/                             # 构建目录




















