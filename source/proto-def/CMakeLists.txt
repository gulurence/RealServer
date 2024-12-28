# 创建 protocol 静态库
add_library(protocol STATIC 
    main.cpp
)

# 设置头文件路径
target_include_directories(protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
