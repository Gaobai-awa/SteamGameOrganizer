#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <set>

// ============================================================
// 游戏数据结构
// 存储从 Steam 本地文件读取到的所有游戏信息
// ============================================================
struct GameInfo {
    uint32_t    appid       = 0;        // Steam AppID
    std::string name;                    // 游戏名称 (UTF-8)
    std::string install_dir;             // 安装目录名
    std::string library_path;            // 所在库文件夹完整路径
    uint64_t    size_on_disk = 0;        // 占用磁盘空间 (字节)
    uint64_t    last_updated = 0;        // 最后更新时间 (Unix时间戳)
    int         state_flags  = 0;        // Steam 状态标记
    bool        is_dlc       = false;    // 是否为 DLC
    bool        is_installed = true;     // 是否已安装
    std::string app_type;                // App 类型: game / dlc / tool / music / video / ...



    // --- 以下来自 localconfig.vdf (用户游玩数据) ---
    uint64_t    total_playtime_min       = 0;  // Steam 记录的总游玩时长 (分钟)
    uint64_t    playtime_disconnected_min = 0; // 离线/无网时长的分钟数 (PlaytimeDisconnected)
    uint64_t    last_played              = 0;  // 最后启动时间 (Unix时间戳)
};

// ============================================================
// 分类夹结构
// 一个分类夹包含多个游戏的 appid
// 一个游戏可以属于多个分类夹 (多对多)
// ============================================================
struct Category {
    std::string         name;           // 分类名称
    std::string         parent;         // 父分类名 (空 = 根分类)
    std::set<uint32_t>  game_ids;       // 该分类下的游戏 AppID 集合
};

// ============================================================
// 游玩时长记录 (单次会话)
// ============================================================
struct PlaySession {
    uint32_t appid;
    int64_t  start_time;    // Unix 时间戳
    int64_t  end_time;      // Unix 时间戳 (0 = 仍在运行)
    int64_t  duration_sec;  // 秒数
};
