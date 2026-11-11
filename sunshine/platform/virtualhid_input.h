// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/platform/common.h"
// Compatibility for retained input.cpp's refresh hook. No local devices exist.
namespace platf::virtualhid {
struct input_context_t {
  void refresh_keyboard() {}
  void refresh_mouse() {}
};
input_context_t &get_input_context(input_t &input);
}
