// 调试二进制 VDF 名称提取
#include "binary_vdf.h"
#include <iostream>
#include <windows.h>

void dump_node(const BinaryKVNode& node, int depth = 0) {
    for (int i = 0; i < depth; i++) std::cout << "  ";
    std::cout << node.name << ": ";
    if (node.is_object) {
        std::cout << "{ object, " << node.children.size() << " children }\n";
        for (const auto& c : node.children) dump_node(c, depth + 1);
    } else if (!node.string_value.empty()) {
        std::cout << "\"" << node.string_value << "\"\n";
    } else if (node.int_value != 0) {
        std::cout << node.int_value << "\n";
    } else {
        std::cout << "(empty)\n";
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    
    BinaryVDFReader reader;
    if (!reader.open_appinfo("e:/steam/appcache/appinfo.vdf")) {
        std::cerr << "open fail\n"; return 1;
    }
    
    // 找 CS2
    auto* cs2 = reader.find_app(730);
    if (!cs2) { std::cerr << "CS2 not found\n"; return 1; }
    
    std::cout << "CS2 (730) KV tree:\n";
    dump_node(cs2->data);
    std::cout << "\nName: \"" << BinaryVDFReader::get_app_name(*cs2) << "\"\n";
    
    // 找 Steam 常用工具
    for (const auto& app : reader.apps()) {
        if (app.appid == 228980 || app.appid == 250820 || app.appid == 400) {
            std::cout << "\nApp " << app.appid << " KV tree:\n";
            dump_node(app.data);
            std::cout << "Name: \"" << BinaryVDFReader::get_app_name(app) << "\"\n";
        }
    }
    
    return 0;
}
