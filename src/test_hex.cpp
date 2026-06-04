// 查看 appinfo.vdf 条目数据
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cctype>

int main() {
    FILE* f = fopen("e:/steam/appcache/appinfo.vdf", "rb");
    if (!f) { printf("open fail\n"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz);
    fread(d.data(), 1, sz, f);
    fclose(f);
    
    size_t pos = 0;
    uint32_t magic = *(uint32_t*)&d[pos]; pos += 4;
    int ver = magic & 0xFF;
    // magic: 0x07564429 -> bytes: 29 44 56 07
    // ver = 0x29 = 41
    printf("magic=0x%08X ver=%d\n", magic, ver);
    
    pos += 4; // universe
    if (ver >= 41) {
        uint64_t str_off = *(uint64_t*)&d[pos];
        pos += 8;
        printf("string_table_offset=0x%zX\n", (size_t)str_off);
    }
    
    // 读第一个条目
    uint32_t appid = *(uint32_t*)&d[pos];
    uint32_t entry_sz = *(uint32_t*)&d[pos + 4];
    printf("\n[0] appid=%u entry_size=%u (0x%X)\n", appid, entry_sz, entry_sz);
    
    size_t entry_start = pos;
    pos += 8; // 跳过 appid + size
    
    uint32_t info_state = *(uint32_t*)&d[pos]; pos += 4;
    uint32_t last_updated = *(uint32_t*)&d[pos]; pos += 4;
    uint64_t token = *(uint64_t*)&d[pos]; pos += 8;
    pos += 20; // SHA1 hash
    pos += 20; // BinaryDataHash (ver >= 40)
    uint32_t change_num = *(uint32_t*)&d[pos]; pos += 4;
    
    printf("info_state=%u last_updated=%u token=0x%zX change_num=%u\n", info_state, last_updated, (size_t)token, change_num);
    printf("data starts at offset 0x%zX from file start\n", pos);
    printf("data ends at offset 0x%zX (entry_end)\n", entry_start + 8 + entry_sz);
    
    // Hex dump data section
    size_t data_size = (entry_start + 8 + entry_sz) - pos;
    if (data_size > 128) data_size = 128;
    printf("\nData hex dump (%zu bytes):\n", data_size);
    
    for (size_t i = 0; i < data_size; i++) {
        printf("%02X ", d[pos + i]);
        if ((i + 1) % 32 == 0) printf("\n");
    }
    printf("\n\nASCII:\n");
    for (size_t i = 0; i < data_size; i++) {
        char c = d[pos + i];
        printf("%c", (c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");
    
    return 0;
}
