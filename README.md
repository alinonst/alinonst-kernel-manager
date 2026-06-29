# alinonst-kernel-manager

[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)
[![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793D1?logo=arch-linux&logoColor=white)](https://archlinux.org)

**alinonst-kernel-manager** 是一个用 C++ 编写的交互式命令行工具，用于在 Arch Linux 上轻松安装和管理各种内核版本。

它自动处理第三方仓库配置、GPG 密钥导入、CPU 微架构检测，并可选自动更新 GRUB 引导配置，让您“零配置”即可安装几乎所有流行的 Arch Linux 内核。

## ✨ 功能特点

- **一键安装**：支持官方内核、AUR 内核、第三方仓库（repo-ck、Chaotic-AUR、CachyOS）内核。
- **自动 CPU 优化**：针对 `linux-xanmod`、`linux-ck` 等内核，自动检测 CPU 微架构（x86-64-v2/v3/v4），安装对应优化包。
- **自动仓库配置**：首次使用时自动配置所需第三方仓库和 GPG 密钥，无需手动编辑 `pacman.conf`。
- **自动依赖安装**：若需安装 AUR 内核但未安装 `yay`，会提示并自动安装。
- **GRUB 集成**：安装完成后可选择自动更新 GRUB 引导配置。
- **交互式 Shell**：提供友好的交互式命令界面，支持命令补全（手动）。
- **命令行模式**：支持脚本化操作（`--install`, `--list`, `--update-grub`, `--setup-repos`）。
- **彩色输出**：清晰展示操作过程和结果。

## 📦 支持的内核

| 内核名称          | 来源               | 说明                         |
|-------------------|-------------------|------------------------------|
| linux             | 官方 Core          | 最新稳定版内核               |
| linux-lts         | 官方 Core          | 长期支持版                   |
| linux-hardened    | 官方 Core          | 安全加固版                   |
| linux-zen         | 官方 Core          | 桌面响应优化版               |
| linux-rt          | 官方 Extra         | 实时内核                     |
| linux-rt-lts      | 官方 Extra         | 实时内核 LTS 版              |
| linux-ck          | repo-ck            | 使用 BFS/MuQSS 调度器        |
| linux-xanmod      | Chaotic-AUR        | 高性能，支持 BBRv3           |
| linux-lqx         | Chaotic-AUR        | Liquorix，桌面多媒体优化     |
| linux-cachyos     | CachyOS 仓库       | BORE 调度器，激进优化        |
| linux-clear       | AUR                | Intel 平台优化               |
| linux-hardened-lts| AUR                | 安全加固 LTS 版              |

> 对于 `linux-xanmod` 和 `linux-ck`，工具会自动选择适合您 CPU 的优化包。

## 🚀 安装方法

### 方法一：从 AUR 安装（推荐）

```bash
yay -S alinonst-kernel-manager



### 方法二：从源码构建

```bash
git clone https://github.com/yourusername/alinonst-kernel-manager.git
cd alinonst-kernel-manager
makepkg -si
```

###方法三：手动编译

```bash
make
sudo make install
```

📖 使用方法

交互式 Shell

直接运行程序（无参数）：

```bash
alinonst-kernel-manager
```

进入交互界面后，可使用以下命令：

· list — 列出所有可用内核
· install <内核名> 或 i <内核名> — 安装指定内核
· update-grub — 手动更新 GRUB 配置
· setup-repos — 手动配置所有第三方仓库（通常无需手动）
· help — 显示帮助
· exit 或 quit — 退出

示例：

```
> list
> install linux-zen
> install linux-xanmod      # 自动检测 CPU，安装对应优化包
> update-grub
> exit
```

命令行参数模式

```bash
alinonst-kernel-manager --list
sudo alinonst-kernel-manager --install linux-zen
sudo alinonst-kernel-manager --install linux-xanmod
alinonst-kernel-manager --update-grub
alinonst-kernel-manager --setup-repos
alinonst-kernel-manager --help
```

⚙️ 配置说明

工具会自动处理所有配置，无需用户干预。但您也可以手动运行 setup-repos 命令来提前配置所有第三方仓库。

🤝 贡献

欢迎提交 Issue 和 Pull Request。请阅读 CONTRIBUTING.md 了解贡献指南。

📜 许可证

本项目使用 AGPL-3.0 许可证。

🙏 致谢

· Arch Linux
· repo-ck
· Chaotic-AUR
· CachyOS
