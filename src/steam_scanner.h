#pragma once
#include "game_data.h"
#include "binary_vdf.h"
#include <string>
#include <vector>
#include <map>

// ============================================================
// Steam 扫描器
// 负责:
//   1. 从注册表查找 Steam 安装路径
//   2. 解析 libraryfolders.vdf 获取所有库文件夹
//   3. 扫描 appmanifest_*.acf 获取游戏列表 (方法 0)
//   4. 解析 appinfo.vdf 获取 Steam 已知的全部 App 列表 (方法 2: 解密)
//   5. 读取 localconfig.vdf 获取用户游玩数据
// ============================================================
class SteamScanner {
public:
    // 查找 Steam 安装目录 (从注册表)
    static std::string find_steam_path();

    // 获取所有 Steam 库文件夹路径
    static std::vector<std::string> get_library_paths(const std::string& steam_path);

    // 扫描所有已安装游戏 (包含 Steam 记录的游玩数据)
    std::vector<GameInfo> scan_all_games();


    // 通过 appinfo.vdf 解密方式获取所有 Steam 已知 App
    // (包含已安装和已卸载的, 通过 common/<installdir> 目录存在与否判断是否已安装)
    // show_tools = true 时保留 type=tool 的条目 (Dedicated Server / SDK 等); 否则过滤掉
    std::vector<GameInfo> scan_from_appinfo(bool show_tools = false);

    // 设置是否显示工具 (true = 显示, false = 隐藏)
    void set_show_tools(bool v) { m_show_tools = v; }
    bool get_show_tools() const { return m_show_tools; }

    // 刷新游戏列表
    void refresh();

    // 获取已扫描的游戏列表
    const std::vector<GameInfo>& games() const { return m_games; }
    std::vector<GameInfo>& games() { return m_games; }

    // 按 appid 查找游戏
    const GameInfo* find_game(uint32_t appid) const;
    GameInfo* find_game(uint32_t appid);

    // 获取 Steam 用户列表和他们的游玩数据
    std::map<uint32_t, std::pair<uint64_t, uint64_t>>
        read_user_playtime(const std::string& steam_path);

private:
    // 解析单个 appmanifest 文件
    GameInfo parse_appmanifest(const std::string& filepath, const std::string& library_path);

    // 从 localconfig.vdf 读取单个用户的游戏数据
    void read_localconfig(const std::string& config_path);

    // 解析 appinfo.vdf 的单个 App 条目, 转成 GameInfo
    GameInfo appinfo_to_gameinfo(const AppInfoEntry& app);

    std::string m_steam_path;
    std::vector<std::string> m_library_paths;
    std::vector<GameInfo> m_games;
    // appid -> {total_playtime_min, last_played}
    std::map<uint32_t, std::pair<uint64_t, uint64_t>> m_playtime_data;
    // 是否显示 type=tool 的 App (Dedicated Server / SDK 等)
    bool m_show_tools = false;
};
