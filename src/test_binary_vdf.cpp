// 测试 appinfo.vdf 解析
#include "binary_vdf.h"
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    
    BinaryVDFReader reader;
    std::cout << "正在解析 appinfo.vdf...\n";
    
    if (!reader.open_appinfo("e:/steam/appcache/appinfo.vdf")) {
        std::cerr << "打开失败!\n";
        return 1;
    }
    
    std::cout << "共 " << reader.apps().size() << " 个 App\n\n";
    
    // 列出前 20 个游戏
    int game_count = 0;
    for (const auto& app : reader.apps()) {
        std::string name = BinaryVDFReader::get_app_name(app);
        if (!name.empty() && name.find("Steam") == std::string::npos && name.find("steam") == std::string::npos) {
            std::cout << "[" << app.appid << "] " << name << "\n";
            if (++game_count >= 30) break;
        }
    }
    
    // 查找已知游戏
    auto* cs2 = reader.find_app(730);
    if (cs2) {
        std::cout << "\n找到 CS2 (730): " << BinaryVDFReader::get_app_name(*cs2) << "\n";
    }
    
    return 0;
}
