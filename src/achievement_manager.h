#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Steam 成就数据结构
struct SteamAchievement {
    std::string name;          // 成就 API 名称
    std::string display_name;  // 显示名称
    std::string description;   // 描述
    bool        unlocked = false;
    int64_t     unlock_time = 0; // 解锁时间戳
    bool        has_local_data = false;
};

class AchievementManager {
public:
    AchievementManager() = default;

    // 尝试从 Steam 本地缓存读取成就数据
    bool load_achievements(uint32_t appid, uint32_t userid, const std::string& steam_path);

    // 获取成就列表
    const std::vector<SteamAchievement>& achievements() const { return m_achievements; }

    // 获取已解锁数
    int unlocked_count() const;

    // 获取总数
    int total_count() const { return (int)m_achievements.size(); }

    // 检查是否有数据
    bool has_data() const { return m_has_data; }

    // 尝试设置成就状态 (需要 Steam 客户端配合)
    bool set_achievement(uint32_t appid, const std::string& name, bool unlocked);

private:
    // 尝试解析 UserGameStats 缓存文件
    bool parse_stats_cache(const std::string& filepath);

    // 尝试解析 UserGameStatsSchema 缓存文件 (获取成就名称)
    bool parse_schema_cache(const std::string& filepath);

    std::vector<SteamAchievement> m_achievements;
    bool m_has_data = false;
};
