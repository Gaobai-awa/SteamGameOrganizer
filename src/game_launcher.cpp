#include "game_launcher.h"
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>

// ============================================================
// Shell 打开 (URL 协议 或 可执行文件)
// ============================================================
bool GameLauncher::shell_open(const std::string& url_or_path, const std::string& params) {
    std::string cmd = url_or_path;
    if (!params.empty()) {
        cmd += " " + params;
    }

    // 使用 ShellExecuteA 打开
    HINSTANCE result = ShellExecuteA(
        nullptr,            // 父窗口
        "open",             // 操作
        url_or_path.c_str(),// 文件/URL
        params.empty() ? nullptr : params.c_str(), // 参数
        nullptr,            // 工作目录
        SW_SHOWNORMAL       // 显示方式
    );

    // ShellExecute 返回值 > 32 表示成功
    INT_PTR ret = reinterpret_cast<INT_PTR>(result);
    if (ret > 32) {
        return true;
    }

    // 如果 ShellExecute 失败
    std::cerr << "[错误] ShellExecute 失败, 错误码: " << ret << "\n";
    return false;
}

// ============================================================
// 使用 steam:// 协议启动游戏
// ============================================================
bool GameLauncher::launch_steam_protocol(uint32_t appid) {
    std::ostringstream url;
    url << "steam://rungameid/" << appid;

    std::cout << "[启动] 使用协议启动游戏 (AppID: " << appid << ")\n";
    std::cout << "[启动] URL: " << url.str() << "\n";

    return shell_open(url.str());
}

// ============================================================
// 使用 steam.exe 直接启动
// 命令: steam.exe -applaunch <appid>
// ============================================================
bool GameLauncher::launch_via_steam_exe(const std::string& steam_exe_path, uint32_t appid) {
    std::ostringstream params;
    params << "-applaunch " << appid;

    std::cout << "[启动] 使用 steam.exe 启动 (AppID: " << appid << ")\n";
    std::cout << "[启动] 路径: " << steam_exe_path << "\n";

    return shell_open(steam_exe_path, params.str());
}

// ============================================================
// 智能启动: 优先使用 steam:// 协议，失败则用 steam.exe
// ============================================================
bool GameLauncher::smart_launch(const GameInfo& game, const std::string& steam_path) {
    std::cout << "[启动] 正在启动: " << game.name << " (AppID: " << game.appid << ")\n";

    // 方式1: 使用 steam:// 协议 (推荐)
    if (launch_steam_protocol(game.appid)) {
        return true;
    }

    std::cerr << "[警告] steam:// 协议失败，尝试使用 steam.exe 直接启动...\n";

    // 方式2: 使用 steam.exe
    std::string steam_exe = steam_path + "/steam.exe";
    return launch_via_steam_exe(steam_exe, game.appid);
}
