# 万加链公链项目

### Mesh Network

Mesh网络也称为"多跳网络"，它是一个动态的可以不断扩展的网络架构，并能有效地在无线设备之间传输。

### DPoS共识算法

区块链上高性能的去中心共识算法。

### 链码（Chain Code）和虚拟机（VM）

开发者可以使用链码在万加链上发行DAPPs，也可以使用链码与链外世界交互等等

### VEX

VEX是万加链实现的跨链操作协议，包含跨链资产交换协议和跨链事务交互协议
#BaaS服务

### DOC
（Digital assets Ownership Certification），是万加链结合IPFS开发出的数字资产所有权认证功能，用来对万加链上的物联网设备所产生的每一条数据进行所有权认证，然后将认证过的数据进行加密存储至IPFS上。

### 智能通讯模组

万加链通过自己开发的硬件模组接入智能硬件，智能硬件内置区块链轻节点，可以实现无网通信、无网操控

### 万加链数据交易所

用户可以使用数据模块来管理自有应用和物联网设备等产生的数据，并且可以用来交易

## 获取代码

下载所有的代码，克隆vkt项目库和子模块.
git clone https://github.com/vankiaio/vkt.git --recursive

如果某个项目库已经被克隆没有 --recursive 标记 ，则可以在repo内运行以下命令来检索子模块：
git submodule update --init --recursive

### 自动化编译脚本
自动化编译脚本首先会检查和安装需要的依赖库,然后进行编译 vkt. 脚本支持如下所示的操作系统,我们会在将来的版本中支持其他的Linux/Unix版本.

### OS
Amazon 2017.09 及 更高版本.
Centos 7.
Fedora 25 及 更高版本(推荐Fedora 27).
Mint 18.
Ubuntu 18.04 (推荐Ubuntu 18.04).
MacOS Darwin 10.12 及 更高版本 (推荐MacOS 10.13.x).

### 系统需求（针对所有系统）
至少需要 8GB 内存
至少需要 20GB 磁盘空间

### 运行编译脚本
在 vkt 文件夹中运行脚本,命令如下：

cd vkt
./eosio_build.sh
./eosio_install.sh
