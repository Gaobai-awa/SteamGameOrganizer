#pragma once
#include "game_data.h"
#include <string>
#include <vector>
#include <set>

// ============================================================
// 分类管理器
// 功能:
//   - 创建/删除分类夹
//   - 将游戏加入/移出分类夹 (多对多)
//   - 查看某个分类下的所有游戏
//   - 查看某个游戏所属的所有分类
//   - 持久化存储 (JSON 文件)
//
// 存储格式 (data/categories.json):
// {
//     "categories": [
//         {
//             "name": "FPS",
//             "games": [730, 440, 570]
//         },
//         {
//             "name": "多人",
//             "games": [730, 570]
//         }
//     ]
// }
// ============================================================
class CategoryManager {
public:
    CategoryManager() = default;

    // 从文件加载分类数据
    bool load(const std::string& filepath);
    // 保存分类数据到文件
    bool save(const std::string& filepath);

    // --- 分类操作 ---
    bool create_category(const std::string& name, const std::string& parent = "");
    bool delete_category(const std::string& name);
    bool rename_category(const std::string& old_name, const std::string& new_name);

    // --- 游戏分类操作 ---
    bool add_game_to_category(uint32_t appid, const std::string& category_name);
    bool remove_game_from_category(uint32_t appid, const std::string& category_name);
    bool remove_game_from_all(uint32_t appid);

    // --- 查询 ---
    const std::vector<Category>& categories() const { return m_categories; }
    Category* find_category(const std::string& name);

    // 某个游戏的所有分类名
    std::vector<std::string> get_categories_for_game(uint32_t appid) const;

    // 某个分类下的所有 game id
    const std::set<uint32_t>* get_games_in_category(const std::string& name) const;

private:
    std::vector<Category> m_categories;
    std::string m_filepath;
};
