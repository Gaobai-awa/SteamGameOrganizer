#include "category_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

// ============================================================
// 简单的 JSON 序列化 (不依赖第三方库)
// 手动序列化/反序列化 categories.json
// ============================================================

// JSON 转义
static std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:   oss << c;      break;
        }
    }
    return oss.str();
}

// 简单的 JSON 解析辅助: 跳过空白
static void skip_ws(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
}

// 读取 JSON 字符串
static std::string read_json_str(const std::string& s, size_t& pos) {
    if (pos >= s.size() || s[pos] != '"') return "";
    ++pos; // 跳过开始引号
    std::string val;
    while (pos < s.size()) {
        if (s[pos] == '"') {
            ++pos;
            return val;
        }
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case '"': val += '"'; ++pos; break;
                case '\\': val += '\\'; ++pos; break;
                case 'n': val += '\n'; ++pos; break;
                case 'r': val += '\r'; ++pos; break;
                case 't': val += '\t'; ++pos; break;
                default: val += s[pos++]; break;
            }
        }
        else {
            val += s[pos++];
        }
    }
    return val;
}

// 读取数字
static int read_json_int(const std::string& s, size_t& pos) {
    skip_ws(s, pos);
    std::string num;
    while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '-')) {
        num += s[pos++];
    }
    if (num.empty()) return 0;
    return std::stoi(num);
}

// ============================================================
// 加载
// ============================================================
bool CategoryManager::load(const std::string& filepath) {
    m_filepath = filepath;
    m_categories.clear();

    std::ifstream f(filepath);
    if (!f) {
        // 文件不存在，从空开始
        return true;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    f.close();

    // 简单的 JSON 解析
    size_t pos = 0;
    skip_ws(content, pos);

    // 期待 "categories"
    if (read_json_str(content, pos) != "categories") {
        std::cerr << "[警告] categories.json 格式错误\n";
        return false;
    }
    skip_ws(content, pos);
    if (pos < content.size() && content[pos] == ':') ++pos;

    // 期待 [
    skip_ws(content, pos);
    if (pos < content.size() && content[pos] == '[') {
        ++pos;

        // 解析每个分类对象
        while (pos < content.size()) {
            skip_ws(content, pos);
            if (pos >= content.size()) break;
            if (content[pos] == ']') { ++pos; break; }
            if (content[pos] == ',') { ++pos; continue; }

            // 期待 { "name": "...", "games": [...] }
            if (content[pos] == '{') {
                ++pos;
                Category cat;

                // 读 "name"
                skip_ws(content, pos);
                if (read_json_str(content, pos) != "name") break;
                skip_ws(content, pos);
                if (pos < content.size() && content[pos] == ':') ++pos;
                skip_ws(content, pos);
                cat.name = read_json_str(content, pos);

                // 可能读 "parent"
                skip_ws(content, pos);
                if (content[pos] == ',') ++pos;
                skip_ws(content, pos);
                {
                    size_t save = pos;
                    std::string key = read_json_str(content, pos);
                    if (key == "parent") {
                        skip_ws(content, pos);
                        if (pos < content.size() && content[pos] == ':') ++pos;
                        skip_ws(content, pos);
                        cat.parent = read_json_str(content, pos);
                        skip_ws(content, pos);
                        if (content[pos] == ',') ++pos;
                    } else {
                        pos = save; // 不是 parent 字段，回退
                    }
                }

                // 读 "games"
                skip_ws(content, pos);
                if (read_json_str(content, pos) == "games") {
                    skip_ws(content, pos);
                    if (pos < content.size() && content[pos] == ':') ++pos;
                    skip_ws(content, pos);
                    if (pos < content.size() && content[pos] == '[') {
                        ++pos;
                        while (pos < content.size()) {
                            skip_ws(content, pos);
                            if (content[pos] == ']') { ++pos; break; }
                            if (content[pos] == ',') { ++pos; continue; }
                            int id = read_json_int(content, pos);
                            if (id > 0) cat.game_ids.insert(static_cast<uint32_t>(id));
                        }
                    }
                }

                // 跳到 }
                skip_ws(content, pos);
                if (pos < content.size() && content[pos] == '}') ++pos;

                if (!cat.name.empty()) {
                    m_categories.push_back(cat);
                }
            }
        }
    }

    return true;
}

// ============================================================
// 保存
// ============================================================
bool CategoryManager::save(const std::string& filepath) {
    std::string out_path = filepath.empty() ? m_filepath : filepath;
    if (!out_path.empty()) m_filepath = out_path;
    
    std::ofstream f(out_path, std::ios::trunc);
    if (!f) {
        std::cerr << "[错误] 无法写入 " << out_path << "\n";
        return false;
    }

    f << "{\n";
    f << "    \"categories\": [\n";

    for (size_t i = 0; i < m_categories.size(); ++i) {
        const auto& cat = m_categories[i];
        f << "        {\n";
        f << "            \"name\": \"" << json_escape(cat.name) << "\",\n";
        if (!cat.parent.empty()) {
            f << "            \"parent\": \"" << json_escape(cat.parent) << "\",\n";
        }
        f << "            \"games\": [";

        bool first = true;
        for (uint32_t gid : cat.game_ids) {
            if (!first) f << ", ";
            f << gid;
            first = false;
        }
        f << "]\n";
        f << "        }";
        if (i + 1 < m_categories.size()) f << ",";
        f << "\n";
    }

    f << "    ]\n";
    f << "}\n";
    f.close();

    return true;
}

// ============================================================
// 分类操作
// ============================================================
bool CategoryManager::create_category(const std::string& name, const std::string& parent) {
    for (const auto& cat : m_categories) {
        if (cat.name == name) {
            return false; // 已存在
        }
    }
    // 检查 parent 存在 (如果指定了且非空)
    if (!parent.empty()) {
        bool found = false;
        for (const auto& cat : m_categories) {
            if (cat.name == parent) { found = true; break; }
        }
        if (!found) return false; // 父分类不存在
    }
    Category cat;
    cat.name = name;
    cat.parent = parent;
    m_categories.push_back(cat);
    return true;
}

// 递归删除子分类
static void delete_children(std::vector<Category>& cats, const std::string& parent_name) {
    for (size_t i = 0; i < cats.size();) {
        if (cats[i].parent == parent_name) {
            std::string child = cats[i].name;
            cats.erase(cats.begin() + i);
            delete_children(cats, child);
        } else {
            ++i;
        }
    }
}

bool CategoryManager::delete_category(const std::string& name) {
    auto it = std::find_if(m_categories.begin(), m_categories.end(),
        [&](const Category& c) { return c.name == name; });
    if (it != m_categories.end()) {
        // 先删除所有子分类
        std::string child_name = it->name;
        m_categories.erase(it);
        delete_children(m_categories, child_name);
        return true;
    }
    return false;
}

bool CategoryManager::rename_category(const std::string& old_name, const std::string& new_name) {
    auto* cat = find_category(old_name);
    if (!cat) return false;
    cat->name = new_name;
    // 同步更新所有子分类的 parent 字段
    for (auto& c : m_categories) {
        if (c.parent == old_name) c.parent = new_name;
    }
    return true;
}

// ============================================================
// 游戏分类操作
// ============================================================
bool CategoryManager::add_game_to_category(uint32_t appid, const std::string& category_name) {
    auto* cat = find_category(category_name);
    if (!cat) return false;
    cat->game_ids.insert(appid);
    return true;
}

bool CategoryManager::remove_game_from_category(uint32_t appid, const std::string& category_name) {
    auto* cat = find_category(category_name);
    if (!cat) return false;
    cat->game_ids.erase(appid);
    return true;
}

bool CategoryManager::remove_game_from_all(uint32_t appid) {
    for (auto& cat : m_categories) {
        cat.game_ids.erase(appid);
    }
    return true;
}

// ============================================================
// 查询
// ============================================================
Category* CategoryManager::find_category(const std::string& name) {
    for (auto& cat : m_categories) {
        if (cat.name == name) return &cat;
    }
    return nullptr;
}

std::vector<std::string> CategoryManager::get_categories_for_game(uint32_t appid) const {
    std::vector<std::string> result;
    for (const auto& cat : m_categories) {
        if (cat.game_ids.find(appid) != cat.game_ids.end()) {
            result.push_back(cat.name);
        }
    }
    return result;
}

const std::set<uint32_t>* CategoryManager::get_games_in_category(const std::string& name) const {
    for (const auto& cat : m_categories) {
        if (cat.name == name) return &cat.game_ids;
    }
    return nullptr;
}
