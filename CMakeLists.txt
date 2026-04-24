cmake_minimum_required(VERSION 3.10)
project(deepin-traffic-toll VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# 源文件
set(SOURCES
    src/main.cpp
    src/cli.cpp
    src/config.cpp
    src/process.cpp
    src/tc.cpp
    src/speedtest.cpp
    src/logger.cpp
    src/utils.cpp
)

# 可执行文件
add_executable(deepin-traffic-toll ${SOURCES})

# 链接 pthread
target_link_libraries(deepin-traffic-toll PRIVATE pthread)

# 安装目标
install(TARGETS deepin-traffic-toll DESTINATION bin)

# 复制示例配置
install(FILES example.yaml DESTINATION etc/deepin-traffic-toll)
