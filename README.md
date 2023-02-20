# SimpleNet

使用提供的Makefile文件编译代码.  

在命令行执行"make"将同时编译客户和服务器.  

执行"make client"将只编译客户端.

执行"make server"将只编译服务器.

执行"./lab5-1_client"运行客户端.

执行"./lab5-1_server"运行服务器.

client目录:

app_client.c - 客户端应用程序源文件

stcp_client.c - stcp客户端源文件

stcp_client.h - stcp客户端头文件

server目录:

app_server.c - 服务器应用程序源文件

stcp_server.c - stcp服务器源文件

stcp_server.h - stcp服务器头文件

common目录:

seg.c - sip函数实现

seg.h - sip函数头文件

constants.h - 定义一些有用的常量