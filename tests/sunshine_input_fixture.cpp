// SPDX-License-Identifier: GPL-3.0-or-later
// Production platform -> bridge -> HidClient -> fake private UDS, no devices.
#include "src/rkmoon/rkmoon_bridge.hpp"
#include "src/platform/virtualhid_input.h"
#include "src/logging.h"
#include <filesystem>
#include <thread>
#include <chrono>
int main(int argc,char**argv) {
  if(argc!=5)return 2;
  auto log_guard=logging::init(0,argv[3]); // Verbose: input content must remain absent.
  bool absolute=std::string_view(argv[2])=="absolute";
  rkmoon_sunshine::input_session_t session(argv[1],absolute);
  auto input=platf::input();
  auto client=platf::allocate_client_input_context(input);
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
  while(!std::filesystem::exists(argv[4])) {
    if(std::chrono::steady_clock::now()>deadline)return 3;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // The fake peer marks hello before the production constructor publishes the lease.
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  platf::virtualhid::get_input_context(input).refresh_keyboard();
  platf::virtualhid::get_input_context(input).refresh_mouse();
  platf::keyboard_update(input,81,false,0);
  platf::button_mouse(input,2,false);
  if(absolute)platf::abs_mouse(input,{0,0,1920,1080,0,0},480,810);
  else platf::move_mouse(input,713,-619);
  platf::scroll(input,240);platf::hscroll(input,-120);
  platf::keyboard_update(input,81,true,0);platf::button_mouse(input,2,true);
  platf::unicode(input,"SYNTHETIC_UNICODE_NEVER_LOG",27);
  platf::touch_update(client.get(),{},{});platf::pen_update(client.get(),{},{});
  if(platf::alloc_gamepad(input,{}, {},{})!=-1 || platf::rebind_gamepad(input,{},{})!=-1)return 4;
  if(platf::get_capabilities()!=0)return 5;
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  return 0;
}
