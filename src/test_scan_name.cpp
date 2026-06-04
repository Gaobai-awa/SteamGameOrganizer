// 直接在二进制数据中扫 name 字段
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>

int main() {
    FILE* f = fopen("e:/steam/appcache/appinfo.vdf", "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz);
    fread(d.data(), 1, sz, f);
    fclose(f);
    
    size_t pos = 0;
    uint32_t magic = *(uint32_t*)&d[pos]; pos += 4;
    int ver = magic & 0xFF;
    pos += 4;
    if (ver >= 41) pos += 8;
    
    int found = 0, total = 0;
    while (pos + 8 <= sz) {
        uint32_t appid = *(uint32_t*)&d[pos];
        if (appid == 0) break;
        uint32_t entry_sz = *(uint32_t*)&d[pos + 4];
        size_t entry_end = pos + 8 + entry_sz;
        total++;
        
        // 跳到 KV data 区域
        size_t dp = pos + 8 + 4 + 4 + 8 + 20 + 4;
        if (ver >= 40) dp += 20;
        if (dp + 6 > entry_end) { pos = entry_end; continue; }
        
        // 在二进制 KV 数据中搜索 name 字符串
        size_t se = entry_end < dp + 2048 ? entry_end : dp + 2048;
        for (size_t i = dp; i + 6 <= se; i++) {
            if (d[i]==0x01 && d[i+1]=='n' && d[i+2]=='a' && d[i+3]=='m' && d[i+4]=='e' && d[i+5]==0) {
                std::string n;
                for (size_t j = i+6; j < se && d[j]; j++) n += (char)d[j];
                if (!n.empty()) {
                    printf("[%5u] %s\n", appid, n.c_str());
                    found++;
                }
                break;
            }
        }
        pos = entry_end;
    }
    printf("\n共 %d 个条目, 找到 %d 个名称\n", total, found);
    return 0;
}
