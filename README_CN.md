# Deepin Traffic Toll

基于 Linux TC 的进程级网络流量控制工具，支持带宽限制和流量优先级管理。

## 概述

Deepin Traffic Toll 是一个 C++ 实现的网络流量控制工具，利用 Linux Traffic Control (TC) 和 HTB (Hierarchy Token Bucket) 队列规则，实现对特定应用程序的精细化带宽管理。您可以针对不同进程设置上传/下载速度限制，并分配流量优先级。

## 功能特性

- **进程级带宽限制** - 为特定应用程序设置上传/下载速率限制
- **流量优先级控制** - 为关键应用程序分配更高的带宽优先级
- **灵活的进程匹配** - 支持通过进程名、可执行文件路径或命令行参数进行正则匹配
- **递归进程匹配** - 自动处理子进程的流量整形（适用于 Electron/Chromium 应用）
- **最小带宽保证** - 确保进程不会被高优先级流量完全占用带宽
- **自动测速** - 可选的自动检测网络速度功能
- **动态配置更新** - 随着进程的启动/停止，动态添加/移除过滤规则

## 系统要求

- 支持 TC (Traffic Control) 的 Linux 内核
- IFB (Intermediate Functional Block) 内核模块
- Root 权限（TC 操作需要）

## 安装

### 从源码编译

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### 依赖项

- CMake 3.10 或更高版本
- 支持 C++17 的编译器
- pthread 库

## 使用方法

```bash
sudo deepin-traffic-toll <网络接口> <配置文件> [选项]
```

### 参数说明

| 参数 | 描述 |
|------|------|
| `网络接口` | 要进行流量整形的网络接口（如 `eth0`、`wlan0`） |
| `配置文件` | YAML 配置文件路径 |

### 选项

| 选项 | 描述 |
|------|------|
| `-d, --delay <秒>` | 进程检查间隔时间（默认：1.0 秒） |
| `-l, --logging-level <级别>` | 日志级别：TRACE, DEBUG, INFO, SUCCESS, WARNING, ERROR, CRITICAL（默认：INFO） |
| `-s, --speed-test` | 自动检测上传/下载速度 |
| `-h, --help` | 显示帮助信息 |

### 示例

```bash
sudo deepin-traffic-toll wlp2s0 limit.yaml
```

## 配置说明

配置文件使用 YAML 格式。请参考 `example.yaml` 获取详细示例。

### 全局设置

```yaml
# 全局带宽限制
download: 100mbps
upload: 50mbps

# 未分类流量的最小保证带宽
download-minimum: 100kbps
upload-minimum: 10kbps

# 未分类流量的优先级（0 为最高）
download-priority: 1
upload-priority: 1
```

### 进程配置

```yaml
processes:
  "浏览器":
    # 带宽限制
    download: 10mbps
    upload: 5mbps

    # 优先级（0 为最高优先级）
    download-priority: 0
    upload-priority: 0

    # 最小保证带宽
    download-minimum: 100kbps
    upload-minimum: 50kbps

    # 递归匹配子进程（适用于 Electron 应用）
    recursive: true

    # 匹配条件（支持正则表达式）
    match:
      - name: "chrome"
      - exe: "/usr/bin/firefox"
      - cmdline: ".*browser.*"
```

### 速率单位

支持的速率单位：
- `bit`, `kbit`, `mbit`, `gbit` - 比特每秒
- `bps`, `kbps`, `mbps`, `gbps` - 字节每秒

示例：`100mbps`、`500kbps`、`1gbit`

### 匹配条件

| 键名 | 描述 | 示例 |
|------|------|------|
| `name` | 进程名（来自 `/proc/[pid]/comm`） | `chrome` |
| `exe` | 可执行文件路径（来自 `/proc/[pid]/exe`） | `/usr/bin/firefox` |
| `cmdline` | 命令行参数（来自 `/proc/[pid]/cmdline`） | `.*java.*app.jar` |
| `pid` | 进程 ID | `1234` |

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                        主程序                                │
├──────────────┬──────────────┬──────────────┬───────────────┤
│   命令行解析  │  配置加载    │  进程过滤    │   流量控制     │
│              │              │              │               │
│  - device    │  - YAML解析  │  - /proc扫描 │  - HTB队列    │
│  - config    │  - 进程配置  │  - 正则匹配  │  - U32过滤器  │
│  - options   │  - 速率解析  │  - 端口关联  │  - IFB设备    │
└──────────────┴──────────────┴──────────────┴───────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Linux 内核 TC                            │
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │   Ingress   │    │    IFB      │    │   Egress    │     │
│  │  (下载流量) │───▶│  (重定向)   │    │  (上传流量) │     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
│         │                  │                  │             │
│         ▼                  ▼                  ▼             │
│  ┌─────────────────────────────────────────────────┐       │
│  │              HTB 队列 + 流量类别                 │       │
│  │          (基于优先级的带宽分配)                  │       │
│  └─────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

## 工作原理

1. **初始化阶段**
   - 创建 IFB 虚拟设备用于入站流量重定向
   - 在出站和入站（通过 IFB）方向设置 HTB 队列规则
   - 创建根类别并设置全局带宽限制

2. **运行循环**
   - 扫描 `/proc` 文件系统查找匹配的进程
   - 从 `/proc/net/tcp` 和 `/proc/net/tcp6` 解析进程网络连接
   - 根据本地端口动态添加/移除 TC 过滤规则
   - 按配置的间隔时间休眠

3. **流量整形**
   - HTB 类别强制执行带宽限制
   - 优先级决定哪个流量优先获得带宽
   - 最小速率保证防止进程被完全占用

## 技术细节

### HTB (Hierarchy Token Bucket)

HTB 作为主要的队列规则用于流量整形：
- 每个进程获得独立的 HTB 类别
- 类别可以在有空余带宽时从父类别借用
- 优先级决定借用顺序

### IFB (Intermediate Functional Block)

入站流量整形需要重定向到虚拟设备：
- 流量从 ingress 重定向到 IFB 设备
- HTB 队列规则应用于 IFB 实现入站整形

### 进程匹配

进程匹配使用 `/proc` 文件系统：
- `/proc/[pid]/comm` - 进程名
- `/proc/[pid]/exe` - 可执行文件的符号链接
- `/proc/[pid]/cmdline` - 命令行参数
- `/proc/[pid]/task/[tid]/children` - 子进程

## 许可证

本项目为 Deepin/UOS 系统开发。

## 致谢

灵感来源于 [TrafficToll](https://github.com/cryzed/TrafficToll) - 一个 Python 实现的类似功能工具。
