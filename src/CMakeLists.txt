file(GLOB_RECURSE SRC "*.cpp")    # 递归添加当前目录中的所有源文件

add_library(Thread SHARED ${SRC})