使用make命令编译

重叠网络进程应该在四台主机(或虚拟机)上运行以启动重叠网络.

进入son目录并运行./son&

所有son进程应在1分钟内启动好.

在所有son进程启动好后, 启动所有四个节点上的sip进程.

sip进程应在本地重叠网络初始化完并显示"Overlay network: 

waiting for connection from SIP process..."后启动. 

进入sip目录并运行./sip

要杀掉son进程和sip进程: 使用"kill -s 2 进程号"命令.

如果程序使用的端口号已被使用, 程序将退出.
