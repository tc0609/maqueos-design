# MaQueOS

MaQueOS是一个开源的基于LoongArch架构的教学版操作系统。作为一个教学版操作系统，MaQueOS的代码虽然只有1000多行，但是它实现了操作系统最核心的功能子系统：进程管理、内存管理、文件系统、中断管理和外设驱动，并为应用程序提供了16个系统调用接口。


MaQueOS由Linux 0.11内核深度裁剪而来，并完成了从x86架构到LoongArch架构的移植。其中，对所有外设驱动程序(硬盘、键盘和显示器)进行了重写。除此之外，不同于Linux 0.11支持MINIX文件系统、a.out可执行文件和基于管道操作的进程间通信机制，MaQueOS支持自定义的xtfs格式的文件系统、xt格式的可执行文件和基于共享内存的进程间通信机制。


其中，xtfs文件系统是一个极小型文件系统，仅用100多行代码就实现了文件的创建删除、打开关闭和读写操作，以及文件系统挂载功能；xt可执行文件包括一个512字节大小的xt可执行文件头和代码数据被链接在一起的二进制可执行代码，因此对xt可执行文件的加载过程相当简洁；基于共享内存的进程间通信机制的实现也比基于管道操作的进程间通信机制实现机制更简单高效。

## 一、环境部署

1、在虚拟机中安装 ubuntu 20.04

2、更新源

```bash
sudo gedit /etc/apt/sources.list
```

```
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-backports main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu/ focal-security main restricted universe multiverse
```

```bash
sudo apt update
sudo apt upgrade
```

3、安装软件

```bash
sudo apt install libspice-server-dev libsdl2-2.0-0 libfdt-dev libusbredirparser-dev libfuse3-dev libcurl4 build-essential gcc-multilib libpython2.7 libnettle7 git
```

4、克隆仓库

```bash
git clone https://gitee.com/dslab-lzu/maqueos.git
```

## 二、仓库目录介绍

本仓库中包含《MaQueOS：基于龙芯LoongArch架构的教学版操作系统》教材中12章内容对应的实验代码，分别位于code*目录中。
例如第1章的实验代码在code1中，第2章在code2中，以此类推。

```
.
├── cross-tool  // 交叉编译环境
├── README.md
├── code1       // 第1章实验代码
├── code2       // 第2章实验代码
├── ...
└── code12      // 第12章实验代码
    ├── run             // 实验目录
    ├── xtfs            // xtfs目录
    │   ├── bin                 // MaQueOS应用程序
    │   │   ├── asm.h
    │   │   ├── compile.sh
    │   │   ├── create.S            // 创建hello_xt文件
    │   │   ├── destroy.S           // 删除hello_xt文件
    │   │   ├── hello.S             // 测试output系统调用和软件定时器
    │   │   ├── print.S             // 测试带参数的进程的创建
    │   │   ├── read.S              // 从hello_xt文件中读数据
    │   │   ├── share.S             // 测试页例外
    │   │   ├── shmem.S             // 测试共享内存
    │   │   ├── sync.S              // 测试sync系统调用
    │   │   ├── write.S             // 向hello_xt文件中写数据
    │   │   └── xtsh.S              // MaQueOS使用的shell程序（xtsh）
    │   └── src                 // xtfs工具源码
    └── kernel          // MaQueOS源代码目录
        ├── Makefile
        ├── drv                 // 硬盘、键盘、显示器驱动
        │   ├── console.c
        │   ├── disk.c
        │   └── font.c
        ├── excp                // 中断
        │   ├── exception.c
        │   └── exception_handler.S
        ├── fs                  // 文件系统
        │   └── xtfs.c
        ├── include             // 头文件
        │   └── xtos.h
        ├── init                // 初始化
        │   ├── head.S
        │   └── main.c
        ├── mm                  // 内存
        │   └── memory.c
        └── proc                // 进程
            ├── ipc.c
            ├── proc0
            ├── process.c
            └── swtch.S
```

## 二、编译运行调试

进入code*目录中的实验目录 `run` ，编译、运行、调试都在这个目录下进行。

1、编译运行

在 run 目录下，执行以下命令。

```
./run.sh
```

2、调试

调试分为两步，需要两个终端：

（1）在第一个终端中，在 run 目录下，执行以下命令。

```
./run.sh -d
```

（2）在第二个终端中，在 run 目录下，执行以下命令，启动gdb（gdb调试在该终端中进行）。

```
./gdb.sh
```

## 三、开发过程
写出添加的代码或修改的代码的目录以及修改时间，具体格式为：
```
序号：1
目录：code/12/..
修改内容介绍：简要介绍修改或添加的内容及实现的功能。
修改/添加时间：YYYY-MM-DD
修改/添加人：姓名
```





# 以下为LightenOS修改
Zhl:
## 1、修改drv/console.c：
### 添加#define LOOP_PER_MS 1000000UL（每毫秒循环次数）
### 添加void delay_ms(unsigned int ms)实现延时
### 添加void clear_screen()实现清屏
### 添加void print_welcome()打印WLCOME
### 添加void print_lightenos()打印LIGHTENOS
### 将原来在excp文件夹下的timer_interrupt()修改并移到console.c中利用定时器中断实现光标闪烁
### 修改do_keyboard函数支持光标移动
### 设置2个键盘映射数组以及shift键标志def,实现按住shift输入大写
## 2、修改excp/exception.c:
### 添加timer_interrupt1()保证原定时器逻辑，将timer_interrupt()放入timer_interrupt1()中
### 修改do_exception()函数中调用从timer_interrupt()改为timer_interrupt1()
## 3、修改init/main.c:
### 添加print_welcome();
### 添加print_lightenos();
## 4、修改xtfs/bin中各.S文件：
### 将.S文件中xtos改为LightenOS相关
## 5、修改xtfs:
### 在xtfs/src中添加read.c文件用于从硬盘中读取指定文件
### xtfs中添加了read.c生成的read.o二进制文件便于在xtfs目录下使用./read进行测试
### xtfs中添加了test.txt测试文件，内容“read success!”
### 在xtfs中新生成了一个xtfs.img硬盘
HB:

### kill系统调用：

## 1、修改了proc/process.c:

### 添加了 sys_kill 函数;

## 2、修改了xtfs：

### 在xtfs/bin中添加了kill.S；

### 修改了asm.h (添加了kill指令的对应序号);

### 修改了init_img.sh(添加了kill对应的镜像生成语句)

### 基于位图的物理页分配算法优化,计划改成系统调用的方式实现


TC:
###
## 1、修改sys_read函数，使该函数能够支持对多扇区文件的读取
## 1.1、xtfs.c文件中对sys_read函数进行修改
## 1.2、创建cqut.txt（超过512字节的文件）
## 1.3、对read.S文件进行修改
## 1.4、init_img.sh脚本文件中将cqut.txt copy到镜像文件中

## 2、修改sys_write函数，使该函数能够支持对多扇区文件的写入
## 2.1、xtfs.c文件中对sys_write函数进行修改
## 2.2、创建一个空文件tzh.txt，并修改init_img.sh 脚本，将 test.txt copy到镜像
## 2.3、添加read1.S文件，实现对tzh.txt文件的读取
## 2.4、init_img.sh脚本文件中添加read1.S
## 2.5、添加write1.S文件对tzh.txt文件进行写入操作
## 2.4、init_img.sh脚本文件中添加write1.S

## 3、增加类似Linux的ls指令
## 3.1、exception.c中sysycalls中增加sys_ls
## 3.2、xtfs.c文件中增加sys_ls函数
## 3.3、xtos.h文件中增加对sys_ls函数的定义
## 3.4、添加ls.S文件实现ls功能
## 3.5、asm.h文件中添加#define NR_ls 17与sysycalls位置对齐
## 3.6、init_img.sh脚本文件中添加ls.S

## 4、增加返回文件大小的系统调用
## 4.1、exception.c中sysycalls中增加sys_file_size
## 4.2 、xtfs.c文件中增加sys_file_size函数
## 4.3、xtos.h文件中增加对sys_file_size函数的定义
## 4.4、添加文件filesize.S实现读取文件大小功能
## 4.5、asm.h文件中添加#define NR_fileSize 18与sysycalls位置对齐
## 4.6、init_img.sh脚本文件中添加ls.S
## 4.7、xtfs.c增加intToString函数实现数字转字符串
## 4.8、xtos.h文件中增加对intToString函数的定义

## 5、添加返回文件属性的系统调用
## 5.1、exception.c中sysycalls中增加sys_filetype
## 5.2、xtfs.c文件中增加sys_filetype函数
## 5.3、xtos.h文件中增加对sys_filetype函数的定义
## 5.4、添加文件ftype1.S、ftype2.S、ftype3.S、ftype4.S实现读取文件属性功能
## 5.5、asm.h文件中添加#define NR_filetype 19与sysycalls位置对齐
## 5.6、添加测试文件frw.txt、fr.txt、fw.txt
## 5.7、init_img.sh脚本文件中添加ftype1.S、ftype2.S、ftype3.S、ftype4.S、frw.txt、fr.txt、fw.txt
## 5.8、修改sys_open函数，，给inode->type增加3和4两个值，使type字段的含义变为：2表示可读可写文件，3表示只读文件，4表示只写文件。
###