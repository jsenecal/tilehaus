#pragma once
#include "lvgl.h"
#include <string>
#include <vector>

namespace tilehaus {

// Runtime page navigation: one container per page, a back stack, and an auto
// back button. Home is index 0 (empty stack → back button hidden).
struct PageNav {
  std::vector<std::string> names;
  std::vector<lv_obj_t *> containers;
  std::vector<int> stack;   // visited pages, not including the current one
  int current = 0;
  lv_obj_t *back_btn = nullptr;

  void show(int i) {
    if (i < 0 || i >= static_cast<int>(containers.size())) return;
    for (size_t k = 0; k < containers.size(); ++k) {
      if (!containers[k]) continue;
      if (static_cast<int>(k) == i) lv_obj_clear_flag(containers[k], LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(containers[k], LV_OBJ_FLAG_HIDDEN);
    }
    current = i;
    if (back_btn) {
      if (stack.empty()) lv_obj_add_flag(back_btn, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(back_btn);
    }
  }
  int index_of(const std::string &name) const {
    for (size_t k = 0; k < names.size(); ++k) if (names[k] == name) return static_cast<int>(k);
    return -1;
  }
  void go(const std::string &name) {
    const int i = index_of(name);
    if (i < 0 || i == current) return;
    stack.push_back(current);
    show(i);
  }
  void back() {
    if (stack.empty()) return;
    const int i = stack.back();
    stack.pop_back();
    show(i);
  }
};

inline PageNav &page_nav() { static PageNav n; return n; }

}  // namespace tilehaus
