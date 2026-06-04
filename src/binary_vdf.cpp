#include "binary_vdf.h"
#include <windows.h>
#include <cstring>
#include <functional>
#include <iostream>

// 二进制 KV 节点类型 (来自 ValveKeyValue 项目的 KV1BinaryNodeType)
enum KV1Type : uint8_t {
    KV1T_ChildObject = 0,    // 0x00 = Object
    KV1T_String      = 1,    // 0x01 = String
    KV1T_Int32       = 2,    // 0x02 = Int32
    KV1T_Float32     = 3,    // 0x03 = Float32
    KV1T_Pointer     = 4,    // 0x04 = Pointer
    KV1T_WideString  = 5,    // 0x05 = WideString
    KV1T_Color       = 6,    // 0x06 = Color
    KV1T_UInt64      = 7,    // 0x07 = UInt64
    KV1T_End         = 8,    // 0x08 = End of object
    KV1T_Binary      = 9,    // 0x09 = ProbablyBinary (unused)
    KV1T_Int64       = 10,   // 0x0A = Int64
    KV1T_AltEnd      = 11,   // 0x0B = Alternate end marker (with VBKV magic)
};

uint8_t BinaryVDFReader::read_byte() {
    if (m_pos >= m_data.size()) return 0;
    return m_data[m_pos++];
}

uint32_t BinaryVDFReader::read_uint32() {
    if (m_pos + 4 > m_data.size()) return 0;
    uint32_t v = (uint32_t)m_data[m_pos]
               | ((uint32_t)m_data[m_pos+1] << 8)
               | ((uint32_t)m_data[m_pos+2] << 16)
               | ((uint32_t)m_data[m_pos+3] << 24);
    m_pos += 4;
    return v;
}

uint64_t BinaryVDFReader::read_uint64() {
    if (m_pos + 8 > m_data.size()) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)m_data[m_pos+i]) << (i*8);
    m_pos += 8;
    return v;
}

int32_t BinaryVDFReader::read_int32() { return (int32_t)read_uint32(); }
int64_t BinaryVDFReader::read_int64() { return (int64_t)read_uint64(); }
int32_t BinaryVDFReader::read_int32_at(size_t pos) {
    if (pos + 4 > m_data.size()) return 0;
    return (int32_t)((uint32_t)m_data[pos]
                   | ((uint32_t)m_data[pos+1] << 8)
                   | ((uint32_t)m_data[pos+2] << 16)
                   | ((uint32_t)m_data[pos+3] << 24));
}

float BinaryVDFReader::read_float() {
    if (m_pos + 4 > m_data.size()) return 0;
    uint32_t v = read_uint32();
    float f;
    memcpy(&f, &v, 4);
    return f;
}

std::string BinaryVDFReader::read_string() {
    std::string s;
    while (m_pos < m_data.size()) {
        char c = (char)m_data[m_pos++];
        if (c == '\0') break;
        s += c;
    }
    return s;
}

std::wstring BinaryVDFReader::read_widestring() {
    std::wstring s;
    while (m_pos + 2 <= m_data.size()) {
        wchar_t c = (wchar_t)((uint16_t)m_data[m_pos] | ((uint16_t)m_data[m_pos+1] << 8));
        m_pos += 2;
        if (c == L'\0') break;
        s += c;
    }
    return s;
}

// ============================================================
// 读取 key name
//   v41+:  4 字节 LE int32 string table 索引
//   其他:  NUL-terminated UTF-8 字符串
// ============================================================
std::string BinaryVDFReader::read_key_name() {
    if (m_version >= 41 && !m_string_table.empty()) {
        int32_t idx = read_int32();
        if (idx < 0 || (size_t)idx >= m_string_table.size()) {
            return ""; // 越界保护
        }
        return m_string_table[idx];
    }
    return read_string();
}

// ============================================================
// 读取 v41 字符串表
// 字符串表位于 header 指定的偏移处
//   uint32 string_count
//   string_count 个 NUL-terminated UTF-8 字符串
// ============================================================
bool BinaryVDFReader::read_string_table(int64_t offset) {
    if (offset < 0 || (size_t)offset >= m_data.size()) {
        m_last_error = "Invalid string table offset";
        return false;
    }
    m_pos = (size_t)offset;
    uint32_t count = read_uint32();
    // 防止异常: 限制最多 1M 字符串
    if (count > 1000000) {
        m_last_error = "String table count too large";
        return false;
    }
    m_string_table.clear();
    m_string_table.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (m_pos >= m_data.size()) {
            m_last_error = "Truncated string table";
            return false;
        }
        m_string_table.push_back(read_string());
    }
    return true;
}

// ============================================================
// 读取一个 KV 节点 [name, value]
//   type = 0 (ChildObject): [name] + 子节点列表 (直到 End/AltEnd)
//   type = 1 (String):      [name] + NUL-terminated UTF-8 string
//   type = 2 (Int32):       [name] + 4 bytes LE
//   type = 3 (Float32):     [name] + 4 bytes
//   type = 4 (Pointer):     [name] + 4 bytes
//   type = 5 (WideString):  [name] + NUL-terminated wide string
//   type = 6 (Color):       [name] + 4 bytes
//   type = 7 (UInt64):      [name] + 8 bytes LE
//   type = 8 (End):         对象结束
//   type = 10 (Int64):      [name] + 8 bytes LE
//   type = 11 (AltEnd):     对象结束 (有 VBKV magic 时)
//
//   v41+ 中, "name" 是 4 字节 string table 索引
//   其他版本中, "name" 是 NUL-terminated 字符串
// ============================================================
BinaryKVNode BinaryVDFReader::read_kv_node() {
    BinaryKVNode node;
    if (m_pos >= m_data.size()) return node;

    uint8_t type = read_byte();

    if (type == KV1T_End || type == KV1T_AltEnd) {
        return node; // End 标记 - 空节点
    }

    node.name = read_key_name();  // 读 key 名字

    switch (type) {
        case KV1T_ChildObject: {  // 0x00
            node.is_object = true;
            // 读子节点直到 End/AltEnd
            while (m_pos < m_data.size()) {
                uint8_t next_type = (m_pos < m_data.size()) ? m_data[m_pos] : 0;
                if (next_type == KV1T_End || next_type == KV1T_AltEnd) {
                    read_byte(); // 消耗结束标记
                    break;
                }
                auto child = read_kv_node();
                // 子节点为空时 (type=End), 停止循环
                if (child.name.empty() && !child.is_object) {
                    break;
                }
                node.children.push_back(std::move(child));
            }
            break;
        }
        case KV1T_String:
            // 字符串值始终是 NUL-terminated UTF-8 (即使 v41+)
            // (v41+ 只有 key 名字用 string table, string 值还是 inline)
            node.string_value = read_string();
            break;
        case KV1T_WideString: {
            auto ws = read_widestring();
            int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                node.string_value.resize(len - 1);
                WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &node.string_value[0], len, nullptr, nullptr);
            }
            break;
        }
        case KV1T_Int32:
            node.int_value = read_int32();
            break;
        case KV1T_Float32:
            node.int_value = (int32_t)read_float();
            break;
        case KV1T_Pointer:
        case KV1T_Color:
            node.int_value = read_int32();
            break;
        case KV1T_UInt64:
            node.int64_value = (int64_t)read_uint64();
            node.int_value = (int32_t)node.int64_value;
            break;
        case KV1T_Int64:
            node.int64_value = read_int64();
            node.int_value = (int32_t)node.int64_value;
            break;
        case KV1T_Binary:
        default:
            // 未知类型 - 记录警告但继续
            std::cerr << "[警告] 未知 KV 类型: " << (int)type << "\n";
            break;
    }

    return node;
}

// ============================================================
// 打开并解析 appinfo.vdf
// ============================================================
bool BinaryVDFReader::open_appinfo(const std::string& filepath) {
    m_apps.clear();
    m_string_table.clear();
    m_last_error.clear();
    m_pos = 0;
    m_version = 0;

    // 用 ifstream + 二进制读取文件
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f) {
        m_last_error = "无法打开文件: " + filepath;
        return false;
    }
    std::streamsize size = f.tellg();
    if (size < 16) {
        m_last_error = "文件过小, 不是有效的 appinfo.vdf";
        return false;
    }
    f.seekg(0, std::ios::beg);
    m_data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    // ------------------------------------------------------------
    // 解析文件头
    //   uint32 magic            // 0x07564424~0x07564429 (版本号在最低字节)
    //   uint32 universe         // 总是 1
    //   int64  stringTableOff   // v41+ 才存在
    // ------------------------------------------------------------
    if (m_data.size() < 8) {
        m_last_error = "文件头不完整";
        return false;
    }
    uint32_t magic_full = (uint32_t)m_data[0]
                        | ((uint32_t)m_data[1] << 8)
                        | ((uint32_t)m_data[2] << 16)
                        | ((uint32_t)m_data[3] << 24);
    m_version = magic_full & 0xFF;
    uint32_t magic = magic_full >> 8;
    // 期望 magic == 0x075644 ("VD\x07" 字节序)
    if (magic != 0x075644) {
        m_last_error = "无效的 magic header (期望 0x075644, 实际 0x"
                     + [&]() {
                           char buf[16];
                           snprintf(buf, sizeof(buf), "%X", magic);
                           return std::string(buf);
                       }() + ")";
        return false;
    }
    if (m_version < 36 || m_version > 41) {
        m_last_error = "不支持的 appinfo 版本: " + std::to_string(m_version);
        return false;
    }
    m_pos = 4;
    // universe
    read_uint32();
    // m_pos 现在 = 8

    // v41+ 字符串表偏移
    int64_t string_table_offset = 0;
    if (m_version >= 41) {
        if (m_data.size() < 16) {
            m_last_error = "v41+ 缺少 string table 偏移";
            return false;
        }
        // int64 LE (从当前位置 m_pos=8 读取 8 字节)
        uint64_t off = 0;
        for (int i = 0; i < 8; ++i) off |= ((uint64_t)m_data[m_pos+i]) << (i*8);
        m_pos += 8;  // 跳过 string table offset 字段
        string_table_offset = (int64_t)off;
    }

    // ------------------------------------------------------------
    // v41+ 读取字符串表 (临时移动 m_pos, 读取完恢复)
    // ------------------------------------------------------------
    if (m_version >= 41) {
        size_t saved_pos = m_pos;
        if (!read_string_table(string_table_offset)) {
            return false; // m_last_error 已设置
        }
        m_pos = saved_pos;  // 恢复 m_pos 到 app 条目起始处
    }

    // ------------------------------------------------------------
    // 解析每个 App 条目
    //   uint32 appid           // 0 表示文件结束
    //   uint32 size            // 从当前位置到二进制 VDF 末尾的字节数 (v36+)
    //   uint32 info_state
    //   uint32 last_updated
    //   uint64 token           (v38+)
    //   bytes  20              // SHA1 hash (v38+)
    //   uint32 change_number   (v36+)
    //   bytes  20              // BinaryDataHash (v40+)
    //   KV data                // 二进制 VDF
    // ------------------------------------------------------------
    int count = 0;
    while (m_pos + 8 <= m_data.size()) {
        uint32_t appid = read_uint32();
        if (appid == 0) break; // 文件结束标记

        // v36+ 有 size 字段
        uint32_t size = 0;
        if (m_version >= 36) {
            size = read_uint32();
        } else {
            // 旧版没有 size, 需要自己计算 - 这里不支持
            m_last_error = "不支持的旧版格式 (v<36)";
            return false;
        }
        if (size == 0 || m_pos + size > m_data.size()) {
            m_last_error = "条目大小异常 (appid=" + std::to_string(appid) + ", size=" + std::to_string(size) + ")";
            return false;
        }
        size_t entry_end = m_pos + size;

        AppInfoEntry entry;
        entry.appid = appid;
        entry.info_state    = read_uint32();
        entry.last_updated  = read_uint32();

        if (m_version >= 38) {
            entry.token = read_uint64();
            // SHA1 hash (20 字节)
            if (m_pos + 20 <= entry_end) {
                memcpy(entry.hash, &m_data[m_pos], 20);
                m_pos += 20;
            }
        }

        // change_number (v36+ 都有, 但 v38+ 在 hash 之后)
        if (m_pos + 4 <= entry_end) {
            entry.change_number = read_uint32();
        }

        // binary_data_hash (v40+)
        if (m_version >= 40) {
            if (m_pos + 20 <= entry_end) {
                memcpy(entry.binary_hash, &m_data[m_pos], 20);
                m_pos += 20;
            }
        }

        // 读取 KV 根数据 (直到 End/AltEnd)
        if (m_pos < m_data.size()) {
            entry.data = read_kv_node();
        }

        m_apps.push_back(entry);
        count++;

        // 跳到条目末尾对齐
        if (m_pos < entry_end) {
            m_pos = entry_end;
        }
    }

    std::cout << "[信息] appinfo.vdf v" << m_version
              << " 共 " << count << " 个 App, "
              << (m_string_table.empty() ? "无" : std::to_string(m_string_table.size())) + " 字符串\n";
    return true;
}

const AppInfoEntry* BinaryVDFReader::find_app(uint32_t appid) const {
    for (const auto& app : m_apps) {
        if (app.appid == appid) return &app;
    }
    return nullptr;
}

// 递归在 KV 树中按 key 名查找 string 值
static const std::string* kv_find_str(const BinaryKVNode& node, const std::string& key) {
    for (const auto& child : node.children) {
        if (child.name == key && !child.string_value.empty()) {
            return &child.string_value;
        }
    }
    return nullptr;
}

static const BinaryKVNode* kv_find_obj(const BinaryKVNode& node, const std::string& key) {
    for (const auto& child : node.children) {
        if (child.name == key && child.is_object) {
            return &child;
        }
    }
    return nullptr;
}

std::string BinaryVDFReader::get_app_name(const AppInfoEntry& entry) {
    // entry.data 直接就是 "appinfo" 对象
    // 优先: common.name
    if (auto* common = kv_find_obj(entry.data, "common")) {
        if (auto* n = kv_find_str(*common, "name")) {
            return *n;
        }
    }
    if (auto* n = kv_find_str(entry.data, "name")) {
        return *n;
    }
    // 兜底: 任意层级查找第一个 name 字段
    std::function<const std::string*(const BinaryKVNode&)> find_name;
    find_name = [&](const BinaryKVNode& node) -> const std::string* {
        for (const auto& child : node.children) {
            if (child.name == "name" && !child.string_value.empty()) {
                return &child.string_value;
            }
            if (child.is_object) {
                auto* found = find_name(child);
                if (found) return found;
            }
        }
        return nullptr;
    };
    const auto* name = find_name(entry.data);
    return name ? *name : "";
}

std::string BinaryVDFReader::get_app_localized_name(const AppInfoEntry& entry, const std::string& lang) {
    // 优先查找 common.name_localized.<lang> 子键
    auto find_localized = [&](const BinaryKVNode& node, const std::string& key) -> std::string {
        // 节点是 name_localized 这种 KV 对象, 直接找子键
        for (const auto& child : node.children) {
            if (child.name == key && !child.string_value.empty()) {
                return child.string_value;
            }
        }
        return "";
    };
    if (auto* common = kv_find_obj(entry.data, "common")) {
        // 查找 common.name_localized 子对象
        for (const auto& child : common->children) {
            if (child.name == "name_localized" && child.is_object) {
                // 优先: schinese
                std::string s = find_localized(child, lang);
                if (!s.empty()) return s;
                // 其次: tchinese (繁体)
                if (lang != "tchinese") {
                    s = find_localized(child, "tchinese");
                    if (!s.empty()) return s;
                }
                // 再次: english
                s = find_localized(child, "english");
                if (!s.empty()) return s;
                // 任意: 拿第一个非空的
                for (const auto& lc : child.children) {
                    if (!lc.string_value.empty() && lc.is_object == false) {
                        return lc.string_value;
                    }
                }
            }
        }
    }
    // 兜底: 通用 name
    return get_app_name(entry);
}

std::string BinaryVDFReader::get_app_installdir(const AppInfoEntry& entry) {
    if (auto* common = kv_find_obj(entry.data, "common")) {
        if (auto* s = kv_find_str(*common, "installdir")) {
            return *s;
        }
    }
    if (auto* s = kv_find_str(entry.data, "installdir")) {
        return *s;
    }
    return "";
}

std::string BinaryVDFReader::get_app_type(const AppInfoEntry& entry) {
    // entry.data \u672c\u8eab\u5c31\u662f "appinfo" \u5bf9\u8c61, \u76f4\u63a5\u67e5\u627e "common" \u5b50\u8282\u70b9
    if (auto* common = kv_find_obj(entry.data, "common")) {
        if (auto* s = kv_find_str(*common, "type")) {
            return *s;
        }
    }
    if (auto* s = kv_find_str(entry.data, "type")) {
        return *s;
    }
    return "game"; // 默认游戏
}
