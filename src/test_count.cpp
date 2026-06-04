// 快速计数 appinfo.vdf 条目
#include <cstdio>
#include <cstdint>
#include <vector>

int main() {
    FILE* f = fopen("e:/steam/appcache/appinfo.vdf", "rb");
    if (!f) { printf("打开失败\n"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz);
    fread(d.data(), 1, sz, f);
    fclose(f);
    
    size_t pos = 0;
    uint32_t magic = *(uint32_t*)&d[pos]; pos += 4;
    int ver = magic & 0xFF;
    printf("版本: %d, 文件大小: %zu bytes (%.1f MB)\n", ver, sz, sz/1048576.0);
    
    pos += 4; // universe
    if (ver >= 41) pos += 8; // string table
    
    int count = 0;
    while (pos + 8 <= sz) {
        uint32_t appid = *(uint32_t*)&d[pos];
        if (appid == 0) break;
        uint32_t entry_sz = *(uint32_t*)&d[pos + 4];
        pos += 8 + entry_sz;
        count++;
    }
    printf("总计: %d 个 App\n", count);
    return 0;
}
