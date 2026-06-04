#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <sstream>
#include <stdexcept>

// ============================================================
// VDF (Valve Data Format) 解析器
// Steam 使用 VDF 格式存储 appmanifest_*.acf、
// libraryfolders.vdf、localconfig.vdf 等文件
//
// VDF 语法 (简化版 KeyValues):
//   "key" "value"
//   "key"
//   {
//       "subkey" "value"
//       ...
//   }
// ============================================================

// VDF 值的类型
enum class VDFType {
    String,
    Object
};

// VDF 节点可以是字符串值，也可以是子对象
struct VDFNode {
    VDFType type;
    std::string str_value;                          // type == String 时使用
    std::map<std::string, VDFNode> children;        // type == Object 时使用

    VDFNode() : type(VDFType::Object) {}
    explicit VDFNode(const std::string& val) : type(VDFType::String), str_value(val) {}

    // 快捷访问子节点
    const VDFNode& operator[](const std::string& key) const {
        auto it = children.find(key);
        if (it == children.end()) {
            throw std::runtime_error("VDF key not found: " + key);
        }
        return it->second;
    }

    bool has(const std::string& key) const {
        return children.find(key) != children.end();
    }

    // 尝试获取字符串值（带默认值）
    std::string get_str(const std::string& key, const std::string& def = "") const {
        auto it = children.find(key);
        if (it != children.end() && it->second.type == VDFType::String) {
            return it->second.str_value;
        }
        return def;
    }

    // 尝试获取整数值
    uint64_t get_uint64(const std::string& key, uint64_t def = 0) const {
        auto it = children.find(key);
        if (it != children.end() && it->second.type == VDFType::String) {
            try { return std::stoull(it->second.str_value); }
            catch (...) { return def; }
        }
        return def;
    }
};

// VDF 解析器
class VDFParser {
public:
    static VDFNode parse_file(const std::string& filepath);
    static VDFNode parse_string(const std::string& content);

private:
    // 词法分析器
    struct Token {
        enum Type { String, OpenBrace, CloseBrace, End };
        Type type;
        std::string value;
    };

    class Tokenizer {
    public:
        explicit Tokenizer(const std::string& source);
        Token next();
    private:
        const std::string& src;
        size_t pos = 0;
        void skip_ws_and_comments();
        std::string read_string();
    };

    // 语法分析器
    class Parser {
    public:
        explicit Parser(Tokenizer& t);
        VDFNode parse_object();
        VDFNode parse_root();    // 解析顶层: 支持 "key" { ... } 和 { ... } 两种格式
        void parse_key_value_pairs(VDFNode& obj);
    private:
        Tokenizer& tokenizer;
        Token current;

        void advance();
        std::string expect_string();
    };
};
