#include "vdf_parser.h"
#include <cctype>
#include <codecvt>
#include <locale>

#ifdef _WIN32
#include <windows.h>
// 读取 UTF-8 文件内容
static std::string read_utf8_file(const std::string& path) {
    // 先尝试用宽字符打开文件 (支持中文路径)
    std::wstring wpath(path.begin(), path.end());
    HANDLE h = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        // 回退到 fstream (ASCII路径)
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    DWORD size = GetFileSize(h, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0) {
        CloseHandle(h);
        return "";
    }

    std::string content(size, '\0');
    DWORD read = 0;
    ReadFile(h, &content[0], size, &read, nullptr);
    CloseHandle(h);
    content.resize(read);
    return content;
}
#else
static std::string read_utf8_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
#endif

// ============================================================
// Tokenizer 实现
// ============================================================

VDFParser::Tokenizer::Tokenizer(const std::string& source) : src(source) {}

void VDFParser::Tokenizer::skip_ws_and_comments() {
    while (pos < src.size()) {
        char c = src[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++pos;
        }
        else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            // 跳过单行注释
            while (pos < src.size() && src[pos] != '\n') ++pos;
        }
        else {
            break;
        }
    }
}

std::string VDFParser::Tokenizer::read_string() {
    if (pos >= src.size() || src[pos] != '"') {
        // 不是引号开头，读取到空白字符
        std::string val;
        while (pos < src.size() && !std::isspace(static_cast<unsigned char>(src[pos]))) {
            val += src[pos++];
        }
        return val;
    }

    ++pos; // 跳过开始的 "
    std::string val;
    while (pos < src.size()) {
        char c = src[pos];
        if (c == '"') {
            ++pos; // 跳过结束的 "
            return val;
        }
        else if (c == '\\' && pos + 1 < src.size()) {
            // 转义字符
            ++pos;
            char next = src[pos++];
            switch (next) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case 'r':  val += '\r'; break;
                default:   val += '\\'; val += next; break;
            }
        }
        else {
            val += c;
            ++pos;
        }
    }
    return val; // 没找到结束引号，返回已读取内容
}

VDFParser::Token VDFParser::Tokenizer::next() {
    skip_ws_and_comments();

    if (pos >= src.size()) {
        return { Token::End, "" };
    }

    char c = src[pos];
    if (c == '{') {
        ++pos;
        return { Token::OpenBrace, "{" };
    }
    if (c == '}') {
        ++pos;
        return { Token::CloseBrace, "}" };
    }
    // 字符串
    return { Token::String, read_string() };
}

// ============================================================
// Parser 实现
// ============================================================

VDFParser::Parser::Parser(Tokenizer& t) : tokenizer(t), current(t.next()) {}

void VDFParser::Parser::advance() {
    current = tokenizer.next();
}

std::string VDFParser::Parser::expect_string() {
    if (current.type != Token::String) {
        throw std::runtime_error("VDF parse error: expected string, got something else");
    }
    std::string val = current.value;
    advance();
    return val;
}

VDFNode VDFParser::Parser::parse_object() {
    if (current.type == Token::OpenBrace) {
        advance(); // 跳过 {
        VDFNode obj;
        parse_key_value_pairs(obj);
        if (current.type == Token::CloseBrace) {
            advance(); // 跳过 }
        }
        return obj;
    }
    // 不是对象，返回空对象
    return VDFNode();
}

// 解析顶层结构: 支持 "RootKey" { ... } 和 { ... } 两种格式
VDFNode VDFParser::Parser::parse_root() {
    VDFNode root;
    if (current.type == Token::String) {
        // "key" { ... } 格式
        std::string top_key = current.value;
        advance(); // 消费 key
        VDFNode obj = parse_object();
        root.children[top_key] = std::move(obj);
    } else if (current.type == Token::OpenBrace) {
        // { ... } 格式
        root = parse_object();
    }
    // 其他情况返回空 root
    return root;
}

void VDFParser::Parser::parse_key_value_pairs(VDFNode& obj) {
    while (current.type == Token::String) {
        std::string key = current.value;
        advance(); // 消费 key

        if (current.type == Token::String) {
            // key "value" 形式
            obj.children[key] = VDFNode(current.value);
            advance();
        }
        else if (current.type == Token::OpenBrace) {
            // key { ... } 形式
            obj.children[key] = parse_object();
        }
        else if (current.type == Token::CloseBrace || current.type == Token::End) {
            // key 后面没有值，创建一个空的字符串节点
            obj.children[key] = VDFNode("");
            break;
        }
    }
}

// ============================================================
// 公开接口
// ============================================================

VDFNode VDFParser::parse_file(const std::string& filepath) {
    std::string content = read_utf8_file(filepath);
    if (content.empty()) {
        throw std::runtime_error("Cannot read file: " + filepath);
    }
    // 跳过 UTF-8 BOM (如果存在)
    if (content.size() >= 3 && 
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content = content.substr(3);
    }
    return parse_string(content);
}

VDFNode VDFParser::parse_string(const std::string& content) {
    Tokenizer tokenizer(content);
    Parser parser(tokenizer);
    return parser.parse_root();
}
