# Deepin Traffic Toll

A Linux network traffic control tool for per-process bandwidth management and traffic prioritization.

## Overview

Deepin Traffic Toll is a C++ implementation that leverages Linux Traffic Control (TC) with HTB (Hierarchy Token Bucket) qdisc to provide fine-grained bandwidth management on a per-process basis. It allows you to limit upload/download speeds and prioritize traffic for specific applications.

## Features

- **Per-process bandwidth limiting** - Set upload/download rate limits for specific applications
- **Traffic prioritization** - Assign priority levels to ensure critical applications get bandwidth first
- **Flexible process matching** - Match processes by name, executable path, or command line arguments using regular expressions
- **Recursive process matching** - Automatically shape traffic for child processes (useful for Electron/Chromium apps)
- **Minimum bandwidth guarantees** - Ensure processes don't get starved by higher priority traffic
- **Automatic speed detection** - Optionally auto-detect network speed for optimal configuration
- **Hot configuration updates** - Dynamically adds/removes filters as processes start/stop

## Requirements

- Linux kernel with TC (Traffic Control) support
- IFB (Intermediate Functional Block) kernel module
- Root privileges (required for TC operations)

## Installation

### Build from Source

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### Dependencies

- CMake 3.10+
- C++17 compatible compiler
- pthread library

## Usage

```bash
sudo deepin-traffic-toll <device> <config_file> [options]
```

### Arguments

| Argument | Description |
|----------|-------------|
| `device` | Network interface to shape (e.g., `eth0`, `wlan0`) |
| `config_file` | YAML configuration file path |

### Options

| Option | Description |
|--------|-------------|
| `-d, --delay <seconds>` | Interval between process checks (default: 1.0) |
| `-l, --logging-level <level>` | Log level: TRACE, DEBUG, INFO, SUCCESS, WARNING, ERROR, CRITICAL (default: INFO) |
| `-s, --speed-test` | Automatically detect upload/download speed |
| `-h, --help` | Show help message |

### Example

```bash
sudo deepin-traffic-toll wlp2s0 limit.yaml
```

## Configuration

Configuration files use YAML format. See `example.yaml` for a comprehensive example.

### Global Settings

```yaml
# Global bandwidth limits
download: 100mbps
upload: 50mbps

# Guaranteed minimum bandwidth for unclassified traffic
download-minimum: 100kbps
upload-minimum: 10kbps

# Priority for unclassified traffic (0 = highest)
download-priority: 1
upload-priority: 1
```

### Process Configuration

```yaml
processes:
  "Browser":
    # Bandwidth limits
    download: 10mbps
    upload: 5mbps

    # Priority (0 = highest priority)
    download-priority: 0
    upload-priority: 0

    # Minimum guaranteed bandwidth
    download-minimum: 100kbps
    upload-minimum: 50kbps

    # Match child processes recursively (for Electron apps)
    recursive: true

    # Matching conditions (supports regex)
    match:
      - name: "chrome"
      - exe: "/usr/bin/firefox"
      - cmdline: ".*browser.*"
```

### Rate Units

Supported rate units:
- `bit`, `kbit`, `mbit`, `gbit` - bits per second
- `bps`, `kbps`, `mbps`, `gbps` - bytes per second

Examples: `100mbps`, `500kbps`, `1gbit`

### Matching Conditions

| Key | Description | Example |
|-----|-------------|---------|
| `name` | Process name (from `/proc/[pid]/comm`) | `chrome` |
| `exe` | Executable path (from `/proc/[pid]/exe`) | `/usr/bin/firefox` |
| `cmdline` | Command line arguments (from `/proc/[pid]/cmdline`) | `.*java.*app.jar` |
| `pid` | Process ID | `1234` |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Main Application                        │
├──────────────┬──────────────┬──────────────┬───────────────┤
│   CLI Parser │ Config Loader│Process Filter│ Traffic Control│
│              │              │              │               │
│  - device    │  - YAML      │  - /proc     │  - HTB qdisc  │
│  - config    │  - processes │  - regex     │  - U32 filters│
│  - options   │  - rates     │  - ports     │  - IFB device │
└──────────────┴──────────────┴──────────────┴───────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Linux Kernel TC                          │
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │  Ingress    │    │    IFB      │    │   Egress    │     │
│  │  (download) │───▶│  (redirect) │    │  (upload)   │     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
│         │                  │                  │             │
│         ▼                  ▼                  ▼             │
│  ┌─────────────────────────────────────────────────┐       │
│  │              HTB Qdisc + Classes                 │       │
│  │    (Priority-based bandwidth allocation)        │       │
│  └─────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

## How It Works

1. **Setup Phase**
   - Creates IFB virtual device for ingress traffic redirection
   - Sets up HTB qdisc on both egress and ingress (via IFB)
   - Creates root classes with global bandwidth limits

2. **Runtime Loop**
   - Scans `/proc` filesystem to find matching processes
   - Resolves process connections from `/proc/net/tcp` and `/proc/net/tcp6`
   - Dynamically adds/removes TC filters based on local ports
   - Sleeps for configured delay interval

3. **Traffic Shaping**
   - HTB classes enforce bandwidth limits
   - Priority levels determine which traffic gets bandwidth first
   - Minimum rate guarantees prevent starvation

## Technical Details

### HTB (Hierarchy Token Bucket)

HTB is used as the main qdisc for traffic shaping:
- Each process gets its own HTB class
- Classes can borrow bandwidth from parent when available
- Priority determines borrowing order

### IFB (Intermediate Functional Block)

Ingress traffic shaping requires redirecting to a virtual device:
- Traffic is redirected from ingress to IFB device
- HTB qdisc is applied on IFB for ingress shaping

### Process Matching

Process matching uses `/proc` filesystem:
- `/proc/[pid]/comm` - process name
- `/proc/[pid]/exe` - symbolic link to executable
- `/proc/[pid]/cmdline` - command line arguments
- `/proc/[pid]/task/[tid]/children` - child processes

## License

This project is developed for Deepin/UOS system.
