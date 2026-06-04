#pragma once
#include "game_data.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>

// ============================================================
// 游玩时长追踪器
// 功能:
//   - 记录每次启动游戏的开始时间
//   - 追踪游戏进程，检测退出
//   - 记录每次会话的时长
//   - 统计总时长
//   - 持久化到 JSON 文件
//
// 存储格式 (data/playtime.json):
// {
//     "sessions": [
//         {
//             "appid": 730,
//             "start": 1717200000,
//             "end": 1717203600,
//             "duration": 3600
//         }
//     ],
//     "totals": {
//         "730": 12345,
//         "440": 6789
//     }
// }
// ============================================================
class PlaytimeTracker {
public:
    PlaytimeTracker() = default;

    // 加载历史数据
    bool load(const std::string& filepath);
    // 保存数据
    bool save(const std::string& filepath);

    // 开始追踪一个游戏会话
    // 返回 session 的索引
    int start_session(uint32_t appid);

    // 结束一个游戏会话
    // duration_sec: 游玩时长 (秒), -1 表示自动计算
    bool end_session(int session_idx, int64_t duration_sec = -1);

    // 手动添加一条时长记录
    void add_manual_session(uint32_t appid, int64_t start, int64_t end);

    // --- 统计查询 ---
    // 获取某个游戏的总游玩时长 (秒)
    int64_t get_total_playtime(uint32_t appid) const;

    // 获取所有游戏的总游玩时长
    const std::map<uint32_t, int64_t>& all_totals() const { return m_totals; }

    // 获取最近的 N 条会话记录
    std::vector<PlaySession> get_recent_sessions(int count = 20) const;

    // 获取某个游戏的所有会话
    std::vector<PlaySession> get_sessions_for_game(uint32_t appid) const;

    // 获取当前进行中的会话
    const std::vector<PlaySession>& active_sessions() const { return m_active; }

    // 获取当前时间戳
    static int64_t now();

private:
    std::vector<PlaySession> m_sessions;    // 所有历史会话
    std::vector<PlaySession> m_active;      // 当前活跃的会话
    std::map<uint32_t, int64_t> m_totals;   // appid -> 总时长 (秒)
    std::string m_filepath;

    void rebuild_totals();
};
