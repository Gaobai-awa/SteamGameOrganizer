// 查看每个条目 KV data 开头
#include <cstdio>
#include <cstdint>
#include <vector>

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
    
    int shown = 0;
    while (pos + 8 <= sz && shown < 10) {
        uint32_t appid = *(uint32_t*)&d[pos];
        if (appid == 0) break;
        uint32_t entry_sz = *(uint32_t*)&d[pos + 4];
        size_t entry_end = pos + 8 + entry_sz;
        
        size_t dp = pos + 8 + 4 + 4 + 8 + 20 + 20 + 4; // header (BinaryDataHash always present in v40+)
        
        printf("\n[%u] entry_sz=%u kv_data_pos=0x%zX\n", appid, entry_sz, dp);
        printf("  First 64 bytes: ");
        for (size_t i = dp; i < dp + 64 && i < entry_end; i++) {
            if (i > dp) printf(" ");
            printf("%02X", d[i]);
        }
        printf("\n  ASCII: ");
        for (size_t i = dp; i < dp + 64 && i < entry_end; i++) {
            char c = d[i];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
        
        pos = entry_end;
        shown++;
    }
    return 0;
}
