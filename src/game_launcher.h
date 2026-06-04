#pragma once
#include "game_data.h"
#include <string>
#include <functional>

// ============================================================
// 游戏启动器
// 使用 Steam 协议 (steam://rungameid/<appid>) 启动游戏
// 也可以用直接路径启动 (steam.exe -applaunch <appid>)
// ============================================================
class GameLauncher {
public:
    // 使用 steam:// 协议启动游戏 (默认方式)
    // 调用系统 URL 协议处理器
    static bool launch_steam_protocol(uint32_t appid);

    // 使用 steam.exe 直接启动游戏
    // 需要提供 steam.exe 的路径
    static bool launch_via_steam_exe(const std::string& steam_exe_path, uint32_t appid);

    // 智能启动: 优先尝试协议，失败则用 steam.exe
    static bool smart_launch(const GameInfo& game, const std::string& steam_path);

private:
    // Windows: 调用 ShellExecute 打开 URL
    static bool shell_open(const std::string& url_or_path, const std::string& params = "");
};
