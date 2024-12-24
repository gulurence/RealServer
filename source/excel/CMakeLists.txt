# 创建 ecxel 静态库
add_library(ecxel STATIC 
    main.cpp
)

# 设置头文件路径
target_include_directories(ecxel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
