#include "playtime_tracker.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <ctime>

// ============================================================
// 时间工具
// ============================================================
int64_t PlaytimeTracker::now() {
    return static_cast<int64_t>(std::time(nullptr));
}

// ============================================================
// JSON 辅助 (与 category_manager 共用相同格式的手写解析)
// ============================================================
static void skip_ws_pt(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
}

static std::string read_json_str_pt(const std::string& s, size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return "";
    ++pos;
    std::string val;
    while (pos < s.size()) {
        if (s[pos] == '"') { ++pos; return val; }
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case '"': val += '"'; ++pos; break;
                case '\\': val += '\\'; ++pos; break;
                default: val += s[pos++]; break;
            }
        } else {
            val += s[pos++];
        }
    }
    return val;
}

static int64_t read_json_int64_pt(const std::string& s, size_t& pos) {
    skip_ws_pt(s, pos);
    std::string num;
    while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '-')) {
        num += s[pos++];
    }
    if (num.empty()) return 0;
    return std::stoll(num);
}

// ============================================================
// 加载
// ============================================================
bool PlaytimeTracker::load(const std::string& filepath) {
    m_filepath = filepath;
    m_sessions.clear();
    m_totals.clear();

    std::ifstream f(filepath);
    if (!f) return true; // 文件不存在，从头开始

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    f.close();

    size_t pos = 0;
    skip_ws_pt(content, pos);

    // 根对象 {
    if (pos < content.size() && content[pos] == '{') ++pos;

    while (pos < content.size()) {
        skip_ws_pt(content, pos);
        if (content[pos] == '}') break;

        std::string key = read_json_str_pt(content, pos);
        skip_ws_pt(content, pos);
        if (pos < content.size() && content[pos] == ':') ++pos;

        if (key == "sessions") {
            // 解析 sessions 数组
            skip_ws_pt(content, pos);
            if (content[pos] == '[') {
                ++pos;
                while (pos < content.size()) {
                    skip_ws_pt(content, pos);
                    if (content[pos] == ']') { ++pos; break; }
                    if (content[pos] == ',') { ++pos; continue; }

                    if (content[pos] == '{') {
                        ++pos;
                        PlaySession session = {};
                        while (pos < content.size()) {
                            skip_ws_pt(content, pos);
                            if (content[pos] == '}') { ++pos; break; }
                            if (content[pos] == ',') { ++pos; continue; }

                            std::string field = read_json_str_pt(content, pos);
                            skip_ws_pt(content, pos);
                            if (pos < content.size() && content[pos] == ':') ++pos;
                            int64_t val = read_json_int64_pt(content, pos);

                            if (field == "appid")     session.appid = static_cast<uint32_t>(val);
                            if (field == "start")     session.start_time = val;
                            if (field == "end")       session.end_time = val;
                            if (field == "duration")  session.duration_sec = val;
                        }
                        if (session.appid > 0) {
                            m_sessions.push_back(session);
                        }
                    }
                }
            }
        }
        else if (key == "totals") {
            // { "730": 12345, ... }
            skip_ws_pt(content, pos);
            if (content[pos] == '{') {
                ++pos;
                while (pos < content.size()) {
                    skip_ws_pt(content, pos);
                    if (content[pos] == '}') { ++pos; break; }
                    if (content[pos] == ',') { ++pos; continue; }

                    std::string appid_str = read_json_str_pt(content, pos);
                    skip_ws_pt(content, pos);
                    if (pos < content.size() && content[pos] == ':') ++pos;
                    int64_t total = read_json_int64_pt(content, pos);
                    if (!appid_str.empty()) {
                        m_totals[static_cast<uint32_t>(std::stoul(appid_str))] = total;
                    }
                }
            }
        }
        skip_ws_pt(content, pos);
        if (pos < content.size() && content[pos] == ',') ++pos;
    }

    rebuild_totals();
    return true;
}

// ============================================================
// 保存
// ============================================================
bool PlaytimeTracker::save(const std::string& filepath) {
    std::string out_path = filepath.empty() ? m_filepath : filepath;
    if (!out_path.empty()) m_filepath = out_path;

    std::ofstream f(out_path, std::ios::trunc);
    if (!f) {
        std::cerr << "[错误] 无法写入 " << out_path << "\n";
        return false;
    }

    f << "{\n";

    // sessions 数组
    f << "    \"sessions\": [\n";
    for (size_t i = 0; i < m_sessions.size(); ++i) {
        const auto& s = m_sessions[i];
        f << "        {";
        f << "\"appid\": " << s.appid << ", ";
        f << "\"start\": " << s.start_time << ", ";
        f << "\"end\": " << s.end_time << ", ";
        f << "\"duration\": " << s.duration_sec;
        f << "}";
        if (i + 1 < m_sessions.size()) f << ",";
        f << "\n";
    }
    f << "    ],\n";

    // totals
    f << "    \"totals\": {\n";
    size_t count = 0;
    for (const auto& [appid, total] : m_totals) {
        f << "        \"" << appid << "\": " << total;
        if (++count < m_totals.size()) f << ",";
        f << "\n";
    }
    f << "    }\n";

    f << "}\n";
    f.close();
    return true;
}

// ============================================================
// 开始会话
// ============================================================
int PlaytimeTracker::start_session(uint32_t appid) {
    PlaySession session;
    session.appid = appid;
    session.start_time = now();
    session.end_time = 0;
    session.duration_sec = 0;

    m_active.push_back(session);
    return static_cast<int>(m_active.size()) - 1;
}

// ============================================================
// 结束会话
// ============================================================
bool PlaytimeTracker::end_session(int session_idx, int64_t duration_sec) {
    if (session_idx < 0 || static_cast<size_t>(session_idx) >= m_active.size()) {
        return false;
    }

    auto& session = m_active[session_idx];
    session.end_time = now();

    if (duration_sec >= 0) {
        session.duration_sec = duration_sec;
    }
    else {
        session.duration_sec = session.end_time - session.start_time;
    }

    // 确保时长不为负
    if (session.duration_sec < 0) session.duration_sec = 0;

    // 移到历史记录
    m_sessions.push_back(session);

    // 更新总计
    m_totals[session.appid] += session.duration_sec;

    // 从活跃列表移除
    m_active.erase(m_active.begin() + session_idx);

    return true;
}

// ============================================================
void PlaytimeTracker::add_manual_session(uint32_t appid, int64_t start, int64_t end) {
    PlaySession session;
    session.appid = appid;
    session.start_time = start;
    session.end_time = end;
    session.duration_sec = end - start;
    if (session.duration_sec < 0) session.duration_sec = 0;

    m_sessions.push_back(session);
    m_totals[appid] += session.duration_sec;
}

// ============================================================
int64_t PlaytimeTracker::get_total_playtime(uint32_t appid) const {
    auto it = m_totals.find(appid);
    if (it != m_totals.end()) return it->second;
    return 0;
}

std::vector<PlaySession> PlaytimeTracker::get_recent_sessions(int count) const {
    std::vector<PlaySession> result;
    int start = std::max(0, static_cast<int>(m_sessions.size()) - count);
    for (int i = start; i < static_cast<int>(m_sessions.size()); ++i) {
        result.push_back(m_sessions[i]);
    }
    // 倒序 (最近的在前面)
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<PlaySession> PlaytimeTracker::get_sessions_for_game(uint32_t appid) const {
    std::vector<PlaySession> result;
    for (const auto& s : m_sessions) {
        if (s.appid == appid) result.push_back(s);
    }
    return result;
}

void PlaytimeTracker::rebuild_totals() {
    for (const auto& s : m_sessions) {
        m_totals[s.appid] += s.duration_sec;
    }
}
