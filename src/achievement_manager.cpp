#include "achievement_manager.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// Steam 统计缓存文件格式 (简化解析)
// 文件格式: cache\0 + 字段表 + 数据段
// 数据段使用 Valve 的 KV 格式或 protobuf
// ============================================================

static std::vector<uint8_t> read_file_binary(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// 尝试在缓存文件中查找字段
// 格式: NUL-terminated字段名 + 2字节类型 + 数据
static bool find_field_in_cache(const std::vector<uint8_t>& buf,
                                 const std::string& field_name,
                                 std::vector<uint8_t>& out_data) {
    std::string search = field_name + '\0';
    auto it = std::search(buf.begin(), buf.end(),
                          search.begin(), search.end());
    if (it == buf.end()) return false;
    
    // 跳过字段名和NUL
    it += search.size();
    
    // 剩余字节不够
    if (it + 2 > buf.end()) return false;
    
    // 2 字节类型/长度前缀
    uint16_t type = (uint16_t)(*(it+1)) << 8 | (uint16_t)(*it);
    it += 2;
    
    // 数据
    size_t remaining = buf.end() - it;
    if (remaining > 0) {
        out_data.assign(it, it + std::min(remaining, (size_t)4096));
        return true;
    }
    return false;
}

// 尝试解包压缩数据 (如果存在)
static bool decompress_if_needed(std::vector<uint8_t>& data) {
    // 检查是否是 zlib/deflate 头 (0x78 0x01/9C/DA)
    if (data.size() >= 2 && data[0] == 0x78 && 
        (data[1] == 0x01 || data[1] == 0x9C || data[1] == 0xDA)) {
        // zlib 压缩, 需要 zlib.h 来解压
        // 在此简化: 跳过压缩数据
        return false;
    }
    return true; // 未压缩
}

// ============================================================
bool AchievementManager::load_achievements(uint32_t appid, uint32_t userid,
                                            const std::string& steam_path) {
    m_achievements.clear();
    m_has_data = false;

    // 构建文件路径
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s/appcache/stats/UserGameStats_%u_%u.bin",
             steam_path.c_str(), userid, appid);
    
    std::string filepath(path_buf);
    if (!fs::exists(filepath)) return false;

    // 读取文件
    auto buf = read_file_binary(filepath);
    if (buf.empty()) return false;

    // 检查 magic: "cache\0"
    if (buf.size() < 6 || memcmp(buf.data(), "cache", 5) != 0) return false;

    // 尝试找到数据字段
    std::vector<uint8_t> data;
    if (!find_field_in_cache(buf, "data", data)) return false;

    // 尝试解压
    decompress_if_needed(data);

    // 简单的成就检测: 在数据中查找 "achievement" 字样
    // 完整解析需要 protobuf 解析器, 这里只做基础检测
    int achievement_count = 0;
    std::string data_str(data.begin(), data.end());
    
    size_t pos = 0;
    while ((pos = data_str.find("achievement", pos)) != std::string::npos) {
        ++achievement_count;
        pos += 11;
        // 检查附近是否有 unlock 标记
        size_t start = (pos > 30) ? pos - 30 : 0;
        size_t end = std::min(pos + 30, data_str.size());
        std::string context = data_str.substr(start, end - start);
        
        SteamAchievement ach;
        ach.has_local_data = true;
        
        // 尝试找名称
        // 在数据中找完整字符串
        for (size_t i = start; i < end; ++i) {
            if (data_str[i] >= 0x20 && data_str[i] < 0x7F) {
                // ASCII 范围, 可能是名称的一部分
            }
        }
        
        m_achievements.push_back(ach);
    }

    if (!m_achievements.empty()) {
        m_has_data = true;
        // 给每个成就一个默认名称
        for (size_t i = 0; i < m_achievements.size(); ++i) {
            m_achievements[i].name = "achievement_" + std::to_string(i);
            m_achievements[i].display_name = "成就 #" + std::to_string(i + 1);
        }
    }

    return m_has_data;
}

int AchievementManager::unlocked_count() const {
    int count = 0;
    for (const auto& a : m_achievements) {
        if (a.unlocked) ++count;
    }
    return count;
}

bool AchievementManager::set_achievement(uint32_t appid, const std::string& name, bool unlocked) {
    // 修改 Steam 成就需要 Steam 客户端 IPC 或 SteamAPI
    // 只能通过 Steamworks SDK 实现
    // 这里作为占位
    return false;
}
