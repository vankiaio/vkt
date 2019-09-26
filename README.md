
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
./scripts/eosio_build.sh  
./scripts/eosio_install.sh  

# EOSIO - The Most Powerful Infrastructure for Decentralized Applications

[![Build status](https://badge.buildkite.com/370fe5c79410f7d695e4e34c500b4e86e3ac021c6b1f739e20.svg?branch=master)](https://buildkite.com/EOSIO/eosio)

Welcome to the EOSIO source code repository! This software enables businesses to rapidly build and deploy high-performance and high-security blockchain-based applications.

Some of the groundbreaking features of EOSIO include:

1. Free Rate Limited Transactions
1. Low Latency Block confirmation (0.5 seconds)
1. Low-overhead Byzantine Fault Tolerant Finality
1. Designed for optional high-overhead, low-latency BFT finality
1. Smart contract platform powered by WebAssembly
1. Designed for Sparse Header Light Client Validation
1. Scheduled Recurring Transactions
1. Time Delay Security
1. Hierarchical Role Based Permissions
1. Support for Biometric Hardware Secured Keys (e.g. Apple Secure Enclave)
1. Designed for Parallel Execution of Context Free Validation Logic
1. Designed for Inter Blockchain Communication

EOSIO is released under the open source MIT license and is offered “AS IS” without warranty of any kind, express or implied. Any security provided by the EOSIO software depends in part on how it is used, configured, and deployed. EOSIO is built upon many third-party libraries such as WABT (Apache License) and WAVM (BSD 3-clause) which are also provided “AS IS” without warranty of any kind. Without limiting the generality of the foregoing, Block.one makes no representation or guarantee that EOSIO or any third-party libraries will perform as intended or will be free of errors, bugs or faulty code. Both may fail in large or small ways that could completely or partially limit functionality or compromise computer systems. If you use or implement EOSIO, you do so at your own risk. In no event will Block.one be liable to any party for any damages whatsoever, even if it had been advised of the possibility of damage.  

Block.one is neither launching nor operating any initial public blockchains based upon the EOSIO software. This release refers only to version 1.0 of our open source software. We caution those who wish to use blockchains built on EOSIO to carefully vet the companies and organizations launching blockchains based on EOSIO before disclosing any private keys to their derivative software.

There is no public testnet running currently.

---

**If you used our build scripts to install eosio, [please be sure to uninstall](#build-script-uninstall) before using our packages.**

---

#### Mac OS X Brew Install
```sh
$ brew tap eosio/eosio
$ brew install eosio
```
#### Mac OS X Brew Uninstall
```sh
$ brew remove eosio
```

#### Ubuntu 18.04 Package Install
```sh
$ wget https://github.com/eosio/eos/releases/download/v1.8.1/eosio_1.8.1-1-ubuntu-18.04_amd64.deb
$ sudo apt install ./eosio_1.8.1-1-ubuntu-18.04_amd64.deb
```
#### Ubuntu 16.04 Package Install
```sh
$ wget https://github.com/eosio/eos/releases/download/v1.8.1/eosio_1.8.1-1-ubuntu-16.04_amd64.deb
$ sudo apt install ./eosio_1.8.1-1-ubuntu-16.04_amd64.deb
```
#### Ubuntu Package Uninstall
```sh
$ sudo apt remove eosio
```
#### Centos RPM Package Install
```sh
$ wget https://github.com/eosio/eos/releases/download/v1.8.1/eosio-1.8.1-1.el7.x86_64.rpm
$ sudo yum install ./eosio-1.8.1-1.el7.x86_64.rpm
```
#### Centos RPM Package Uninstall
```sh
$ sudo yum remove eosio
```

#### Build Script Uninstall

If you have previously installed EOSIO using build scripts, you can execute `eosio_uninstall.sh` to uninstall.
- Passing `-y` will answer yes to all prompts (does not remove data directories)
- Passing `-f` will remove data directories (be very careful with this)
- Passing in `-i` allows you to specify where your eosio installation is located

## Supported Operating Systems
EOSIO currently supports the following operating systems:  
1. Amazon Linux 2
2. CentOS 7
3. Ubuntu 16.04
4. Ubuntu 18.04
5. MacOS 10.14 (Mojave)

## Resources
1. [Website](https://eos.io)
1. [Blog](https://medium.com/eosio)
1. [Developer Portal](https://developers.eos.io)
1. [StackExchange for Q&A](https://eosio.stackexchange.com/)
1. [Community Telegram Group](https://t.me/EOSProject)
1. [Developer Telegram Group](https://t.me/joinchat/EaEnSUPktgfoI-XPfMYtcQ)
1. [White Paper](https://github.com/EOSIO/Documentation/blob/master/TechnicalWhitePaper.md)
1. [Roadmap](https://github.com/EOSIO/Documentation/blob/master/Roadmap.md)

<a name="gettingstarted"></a>
## Getting Started
Instructions detailing the process of getting the software, building it, running a simple test network that produces blocks, account creation and uploading a sample contract to the blockchain can be found in [Getting Started](https://developers.eos.io/eosio-home/docs) on the [EOSIO Developer Portal](https://developers.eos.io).

## Contributing

[Contributing Guide](./CONTRIBUTING.md)

[Code of Conduct](./CONTRIBUTING.md#conduct)

## License

[MIT](./LICENSE)

## Important

See LICENSE for copyright and license terms.  Block.one makes its contribution on a voluntary basis as a member of the EOSIO community and is not responsible for ensuring the overall performance of the software or any related applications.  We make no representation, warranty, guarantee or undertaking in respect of the software or any related documentation, whether expressed or implied, including but not limited to the warranties or merchantability, fitness for a particular purpose and noninfringement. In no event shall we be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the software or documentation or the use or other dealings in the software or documentation.  Any test results or performance figures are indicative and will not reflect performance under all conditions.  Any reference to any third party or third-party product, service or other resource is not an endorsement or recommendation by Block.one.  We are not responsible, and disclaim any and all responsibility and liability, for your use of or reliance on any of these resources. Third-party resources may be updated, changed or terminated at any time, so the information here may be out of date or inaccurate.
