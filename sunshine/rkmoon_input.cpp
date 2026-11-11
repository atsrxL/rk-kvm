// SPDX-License-Identifier: GPL-3.0-or-later
// USB input goes only to the user-owned HID bridge; no uinput/desktop backend.
#include "rkmoon_bridge.hpp"
#include "src/platform/virtualhid_input.h"
namespace platf {
struct input_raw_t { virtualhid::input_context_t context; };
input_t input() { return {new input_raw_t{}}; }
void freeInput(input_raw_t *input) { delete input; }
std::unique_ptr<client_input_t> allocate_client_input_context(input_t &) {
  return std::make_unique<client_input_t>();
}
virtualhid::input_context_t &virtualhid::get_input_context(input_t &input) { return input->context; }
void move_mouse(input_t &,int x,int y) { rkmoon_sunshine::relative(x,y); }
void abs_mouse(input_t &,const touch_port_t &port,float x,float y) { rkmoon_sunshine::absolute(port,x,y); }
void button_mouse(input_t &,int button,bool release) { rkmoon_sunshine::button(button,release); }
void scroll(input_t &,int distance) { rkmoon_sunshine::scroll(0,distance); }
void hscroll(input_t &,int distance) { rkmoon_sunshine::scroll(distance,0); }
void keyboard_update(input_t &,uint16_t key,bool release,uint8_t flags) { rkmoon_sunshine::key(key,release,flags); }
void unicode(input_t &,const char *,int) {}
void touch_update(client_input_t *,const touch_port_t &,const touch_input_t &) {}
void pen_update(client_input_t *,const touch_port_t &,const pen_input_t &) {}
int alloc_gamepad(input_t &,const gamepad_id_t &,const gamepad_arrival_t &,feedback_queue_t) { return -1; }
int rebind_gamepad(input_t &,const gamepad_id_t &,feedback_queue_t) { return -1; }
void free_gamepad(input_t &,int) {}
void gamepad_update(input_t &,int,const gamepad_state_t &) {}
void gamepad_touch(input_t &,const gamepad_touch_t &) {}
void gamepad_motion(input_t &,const gamepad_motion_t &) {}
void gamepad_battery(input_t &,const gamepad_battery_t &) {}
platform_caps::caps_t get_capabilities() { return 0; }
std::optional<util::point_t> get_mouse_loc(input_t &) { return std::nullopt; }
std::vector<supported_gamepad_t> &supported_gamepads(input_t *) {
  // Match the pinned backend's null-runtime profile choices; all controllers unavailable.
  static std::vector<supported_gamepad_t> choices {
    {"auto",true,""},{"generic",false,""},{"x360",false,""},{"xone",false,""},
    {"xseries",false,""},{"ds4",false,""},{"ds5",false,""},{"switch",false,""}};
  return choices;
}
}
