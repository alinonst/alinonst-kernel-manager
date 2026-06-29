// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 alinonst

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cctype>
#include <memory>
#include <array>
#include <cstdio>
#include <signal.h>
#include <unistd.h>

// ====================== 防呆宏 ======================
#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// ====================== 全局控制 ======================
volatile sig_atomic_t g_interrupted = 0;
std::string g_logfile = "/var/log/alinonst-kernel-manager.log";

// ====================== 信号处理 ======================
void signal_handler(int sig) {
    if (sig == SIGINT) {
        g_interrupted = 1;
        std::cout << "\n" << YELLOW << "收到中断信号，正在安全退出..." << RESET << std::endl;
    }
}

// ====================== 辅助函数 ======================

// 检查是否在 Arch Linux 上运行
bool is_arch() {
    std::ifstream os("/etc/os-release");
    if (!os.is_open()) return false;
    std::string line;
    while (std::getline(os, line)) {
        if (line.find("ID=arch") != std::string::npos ||
            line.find("ID=archlinux") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 检查是否以 root 运行（sudo 或 uid=0）
bool is_root() {
    return geteuid() == 0;
}

// 检查网络连通性（尝试访问 archlinux.org）
bool network_ok() {
    int ret = system("curl -Is https://archlinux.org | head -1 | grep -q '200 OK'");
    return ret == 0;
}

// 安全的命令执行（带超时和错误检查）
bool safe_exec(const std::string& cmd, bool silent = false, int timeout_sec = 120) {
    if (g_interrupted) return false;
    if (!silent) std::cout << CYAN << "执行: " << cmd << RESET << std::endl;

    // 使用 timeout 防止卡死
    std::string full_cmd = "timeout " + std::to_string(timeout_sec) + "s " + cmd + " 2>&1";
    int ret = std::system(full_cmd.c_str());
    if (ret == -1) {
        std::cerr << RED << "命令执行失败（系统错误）" << RESET << std::endl;
        return false;
    }
    if (WIFEXITED(ret)) {
        int code = WEXITSTATUS(ret);
        if (code != 0) {
            if (!silent) std::cerr << RED << "命令返回错误码: " << code << RESET << std::endl;
            return false;
        }
        return true;
    } else if (WIFSIGNALED(ret)) {
        std::cerr << RED << "命令被信号终止（可能超时）" << RESET << std::endl;
        return false;
    }
    return false;
}

// 获取命令输出（带超时）
std::string get_output(const std::string& cmd, int timeout_sec = 10) {
    std::array<char, 128> buffer;
    std::string result;
    std::string full_cmd = "timeout " + std::to_string(timeout_sec) + "s " + cmd;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// 检查命令是否存在
bool command_exists(const std::string& cmd) {
    std::string check = "command -v " + cmd + " >/dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

// 备份 pacman.conf
bool backup_pacman_conf() {
    std::ifstream src("/etc/pacman.conf");
    if (!src.is_open()) {
        std::cerr << RED << "无法读取 /etc/pacman.conf" << RESET << std::endl;
        return false;
    }
    std::ofstream dst("/etc/pacman.conf.bak");
    if (!dst.is_open()) {
        std::cerr << RED << "无法创建备份文件" << RESET << std::endl;
        return false;
    }
    dst << src.rdbuf();
    std::cout << GREEN << "已备份 /etc/pacman.conf 至 /etc/pacman.conf.bak" << RESET << std::endl;
    return true;
}

// 检查仓库是否已配置（忽略注释和空行）
bool is_repo_configured(const std::string& repo_name) {
    std::ifstream conf("/etc/pacman.conf");
    if (!conf.is_open()) return false;
    std::string line;
    while (std::getline(conf, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;
        if (line.substr(start).find("[" + repo_name + "]") == 0) {
            return true;
        }
    }
    return false;
}

// ====================== 内核数据结构 ======================
struct KernelInfo {
    std::string name;
    std::string package;
    std::string install_cmd_template;
    bool is_aur;
    bool requires_thirdparty;
    std::string repo_type;
    bool requires_arch_detect;
    std::string arch_detect_cmd;
    std::map<std::string, std::string> arch_package_map;
};

static std::vector<KernelInfo> kernels = {
    // 官方 Core
    {"linux", "linux", "sudo pacman -S %s", false, false, "", false, "", {}},
    {"linux-lts", "linux-lts", "sudo pacman -S %s", false, false, "", false, "", {}},
    {"linux-hardened", "linux-hardened", "sudo pacman -S %s", false, false, "", false, "", {}},
    {"linux-zen", "linux-zen", "sudo pacman -S %s", false, false, "", false, "", {}},
    // 官方 Extra
    {"linux-rt", "linux-rt", "sudo pacman -S %s", false, false, "", false, "", {}},
    {"linux-rt-lts", "linux-rt-lts", "sudo pacman -S %s", false, false, "", false, "", {}},
    // 第三方仓库
    {"linux-ck", "linux-ck", "sudo pacman -S %s", false, true, "repo-ck", true,
     "/lib64/ld-linux-x86-64.so.2 --help 2>/dev/null | grep -o 'x86-64-v[0-9]' | tail -1",
     {{"x86-64-v2", "linux-ck-generic-v2"},
      {"x86-64-v3", "linux-ck-generic-v3"},
      {"x86-64-v4", "linux-ck-generic-v4"}}},
    {"linux-xanmod", "linux-xanmod", "sudo pacman -S %s", false, true, "chaotic-aur", true,
     "curl -s https://dl.xanmod.org/check_x86-64_psabi.sh 2>/dev/null | bash 2>/dev/null | grep -o 'x86-64-v[0-9]' | head -1",
     {{"x86-64-v2", "linux-xanmod-x64v2"},
      {"x86-64-v3", "linux-xanmod-x64v3"},
      {"x86-64-v4", "linux-xanmod-x64v4"}}},
    {"linux-lqx", "linux-lqx", "sudo pacman -S %s", false, true, "chaotic-aur", false, "", {}},
    {"linux-cachyos", "linux-cachyos", "sudo pacman -S %s", false, true, "cachyos", false, "", {}},
    // AUR
    {"linux-clear", "linux-clear", "yay -S %s", true, false, "", false, "", {}},
    {"linux-hardened-lts", "linux-hardened-lts", "yay -S %s", true, false, "", false, "", {}},
};

// ====================== 第三方仓库配置（增强防呆） ======================
bool setup_repo_ck() {
    if (is_repo_configured("repo-ck")) {
        std::cout << "repo-ck 已配置。" << std::endl;
        return true;
    }
    std::cout << YELLOW << "正在配置 repo-ck ..." << RESET << std::endl;
    if (!backup_pacman_conf()) return false;
    std::string add_repo = "sudo sh -c 'echo \"\\n[repo-ck]\\nServer = https://mirrors.cernet.edu.cn/repo-ck/\\$arch\" >> /etc/pacman.conf'";
    if (!safe_exec(add_repo)) return false;
    if (!safe_exec("sudo pacman-key -r 5EE46C4C")) return false;
    if (!safe_exec("sudo pacman-key --lsign-key 5EE46C4C")) return false;
    if (!safe_exec("sudo pacman -Sy")) return false;
    std::cout << GREEN << "repo-ck 配置成功。" << RESET << std::endl;
    return true;
}

bool setup_chaotic_aur() {
    if (is_repo_configured("chaotic-aur")) {
        std::cout << "chaotic-aur 已配置。" << std::endl;
        return true;
    }
    std::cout << YELLOW << "正在配置 chaotic-aur ..." << RESET << std::endl;
    if (!backup_pacman_conf()) return false;
    std::string add_repo = "sudo sh -c 'echo \"\\n[chaotic-aur]\\nServer = https://cdn-mirror.chaotic.cx/chaotic-aur/\\$arch\" >> /etc/pacman.conf'";
    if (!safe_exec(add_repo)) return false;
    if (!safe_exec("sudo pacman-key --recv-key 3056513887B78AEB --keyserver keyserver.ubuntu.com")) return false;
    if (!safe_exec("sudo pacman-key --lsign-key 3056513887B78AEB")) return false;
    if (!safe_exec("sudo pacman -Sy")) return false;
    std::cout << GREEN << "chaotic-aur 配置成功。" << RESET << std::endl;
    return true;
}

bool setup_cachyos() {
    if (is_repo_configured("cachyos")) {
        std::cout << "cachyos 已配置。" << std::endl;
        return true;
    }
    std::cout << YELLOW << "正在配置 cachyos ..." << RESET << std::endl;
    if (!backup_pacman_conf()) return false;
    std::string add_repo = "sudo sh -c 'echo \"\\n[cachyos]\\nServer = https://mirrors.ustc.edu.cn/cachyos/repo/\\$arch/\\$repo\" >> /etc/pacman.conf'";
    if (!safe_exec(add_repo)) return false;
    if (!safe_exec("sudo pacman -S --needed cachyos-keyring")) return false;
    if (!safe_exec("sudo pacman -Sy")) return false;
    std::cout << GREEN << "cachyos 配置成功。" << RESET << std::endl;
    return true;
}

bool setup_all_repos() {
    bool ok = true;
    ok &= setup_repo_ck();
    ok &= setup_chaotic_aur();
    ok &= setup_cachyos();
    return ok;
}

// ====================== 安装 yay（防呆） ======================
bool install_yay() {
    std::cout << YELLOW << "检测到 yay 未安装，这是安装 AUR 内核所必需的。" << RESET << std::endl;
    std::cout << "是否自动安装 yay? (y/N): ";
    std::string answer;
    std::getline(std::cin, answer);
    if (answer != "y" && answer != "Y") {
        std::cout << "已取消安装。" << std::endl;
        return false;
    }
    std::cout << "正在安装 yay ..." << std::endl;
    // 清理旧目录
    safe_exec("sudo rm -rf /tmp/yay-bin", true);
    if (!safe_exec("sudo pacman -S --needed base-devel git", false)) {
        std::cerr << RED << "无法安装 base-devel 或 git，请检查 pacman。" << RESET << std::endl;
        return false;
    }
    if (!safe_exec("git clone https://aur.archlinux.org/yay-bin.git /tmp/yay-bin", false)) {
        std::cerr << RED << "克隆 yay-bin 失败，请检查网络。" << RESET << std::endl;
        return false;
    }
    if (!safe_exec("cd /tmp/yay-bin && makepkg -si --noconfirm", false)) {
        std::cerr << RED << "构建 yay 失败。" << RESET << std::endl;
        return false;
    }
    std::cout << GREEN << "yay 安装成功。" << RESET << std::endl;
    return true;
}

// ====================== CPU 检测（防呆） ======================
std::string detect_arch_level() {
    // 优先 ld-linux
    std::string cmd = "/lib64/ld-linux-x86-64.so.2 --help 2>/dev/null | grep -o 'x86-64-v[0-9]' | tail -1";
    std::string
