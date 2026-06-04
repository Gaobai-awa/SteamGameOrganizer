#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <fstream>

// ============================================================
// 二进制 VDF (KeyValues1Binary) 解析器
// 用于解析 appinfo.vdf 和 packageinfo.vdf
// 参考: https://github.com/ValveResourceFormat/ValveKeyValue (KV1BinaryReader.cs)
//       https://github.com/ValveResourceFormat/SteamAppInfo (AppInfo.cs)
//
// 支持格式:
//   - appinfo.vdf v36 (magic 0x07564424) - 无 size / 无 hash
//   - appinfo.vdf v37 (magic 0x07564425)
//   - appinfo.vdf v38 (magic 0x07564426) - + PICS token + SHA1 hash
//   - appinfo.vdf v39 (magic 0x07564427)
//   - appinfo.vdf v40 (magic 0x07564428) - + binary VDF SHA1 hash
//   - appinfo.vdf v41 (magic 0x07564429) - + string table
// ============================================================

// 二进制 KV 节点
struct BinaryKVNode {
    std::string name;
    std::string string_value;      // 字符串值
    int32_t     int_value = 0;     // 整数值
    int64_t     int64_value = 0;   // 64位整数值
    bool        is_object = false; // 是否为子对象
    std::vector<BinaryKVNode> children;

    // 按名称查找子节点
    const BinaryKVNode* find(const std::string& key) const {
        for (const auto& c : children) {
            if (c.name == key) return &c;
        }
        return nullptr;
    }
};

// appinfo.vdf 中的单个 App 条目
struct AppInfoEntry {
    uint32_t appid = 0;
    uint32_t info_state = 0;
    uint32_t last_updated = 0;
    uint64_t token = 0;
    uint8_t  hash[20] = {};        // SHA1 of original text VDF
    uint8_t  binary_hash[20] = {}; // SHA1 of binary VDF (v40+)
    uint32_t change_number = 0;
    BinaryKVNode data;  // 根 KV 节点
};

// ============================================================
// 二进制 VDF 读取器
// ============================================================
class BinaryVDFReader {
public:
    BinaryVDFReader() = default;

    // 打开 appinfo.vdf 并读取所有 App 条目
    bool open_appinfo(const std::string& filepath);

    // 获取所有 App
    const std::vector<AppInfoEntry>& apps() const { return m_apps; }

    // 按 AppID 查找
    const AppInfoEntry* find_app(uint32_t appid) const;

    // 获取 App 的名称 (从 KV 数据中提取)
    static std::string get_app_name(const AppInfoEntry& entry);
    // 获取本地化名称: 优先 schinese > tchinese > english > 通用 name
    // 读取 common.name_localized.<lang> 字段
    static std::string get_app_localized_name(const AppInfoEntry& entry, const std::string& lang = "schinese");

    // 获取 App 的 installdir 字段
    static std::string get_app_installdir(const AppInfoEntry& entry);

    // 获取 App 的 type 字段 (game/dlc/tool/music/video/...)
    static std::string get_app_type(const AppInfoEntry& entry);

    // 获取最后错误
    const std::string& last_error() const { return m_last_error; }

private:
    std::vector<AppInfoEntry> m_apps;
    std::vector<uint8_t> m_data;
    size_t m_pos = 0;
    int    m_version = 0;        // appinfo 版本 (36~41)
    std::vector<std::string> m_string_table; // v41+ 字符串池
    std::string m_last_error;

    // 读取 n 字节
    uint8_t  read_byte();
    uint32_t read_uint32();
    uint64_t read_uint64();
    int32_t  read_int32();
    int64_t  read_int64();
    float    read_float();
    std::string read_string();         // null-terminated string
    std::wstring read_widestring();    // null-terminated wide string
    int32_t  read_int32_at(size_t pos); // 不移动位置的读取

    // 读取 key name (v41+ 用 string table index, 否则用 null-terminated)
    std::string read_key_name();

    // 读取 v41 字符串表
    bool read_string_table(int64_t offset);

    // 解析二进制 KV 节点
    BinaryKVNode read_kv_node();

    // 读取根级节点序列
    std::vector<BinaryKVNode> read_kv_root();

    // 跳过 n 字节
    void skip(size_t n) { m_pos += n; }
    void seek(size_t pos) { m_pos = pos; }
    size_t tell() const { return m_pos; }
    size_t size() const { return m_data.size(); }
};
