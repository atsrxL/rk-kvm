// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rtsp.h"
#include <atomic>
#include <vector>
namespace proc {
struct ctx_t {std::string name,id;};
class proc_t {
 std::atomic_int active{0};std::vector<ctx_t> apps;
public:
 std::vector<ctx_t>& get_apps() {return apps;}
 int execute(int id,std::shared_ptr<rtsp_stream::launch_session_t>);
 int running() {return active.load();}
 void terminate() {active=0;}
 std::string get_last_run_app_name() {return "HDMI";}
 std::string get_app_image(int) {return "assets/box.png";}
};
extern proc_t proc;
struct deinit_t {};
std::unique_ptr<deinit_t> init();
}
