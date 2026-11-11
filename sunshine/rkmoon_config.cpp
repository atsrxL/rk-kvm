// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/config.h"
#include "src/platform/common.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <cmath>
#include <sstream>
#include <vector>
using namespace std::chrono_literals;
namespace config {
video_t video;
audio_t audio;
stream_t stream;
nvhttp_t nvhttp;
sunshine_t sunshine;
  input_t input {
    {
      {0x10, 0xA0},
      {0x11, 0xA2},
      {0x12, 0xA4},
    },
    -1ms,  // back_button_timeout
    500ms,  // key_repeat_delay
    std::chrono::duration<double> {1 / 24.9},  // key_repeat_period

    "auto",  // Default gamepad
    {},  // gamepad_driver remains unset until the user chooses a Windows driver policy
    true,  // back as touchpad click enabled for PlayStation-style gamepads
    true,  // client gamepads with motion events use PlayStation-style emulation
    true,  // client gamepads with touchpads use PlayStation-style emulation
    true,  // virtualhid_randomize_mac

    true,  // keyboard enabled
    false,  // key_rightalt_to_key_win
    true,  // mouse enabled
    true,  // controller enabled
    true,  // always send scancodes
    true,  // high resolution scrolling
    true,  // native pen/touch support
  };
namespace {
std::string trim(std::string s) {auto b=s.find_first_not_of(" \t\r\n");return b==s.npos?"":s.substr(b,s.find_last_not_of(" \t\r\n")-b+1);}
int number(const std::string& s,int lo,int hi) {size_t n; auto v=std::stoi(s,&n);if(n!=s.size()||v<lo||v>hi)throw std::runtime_error("Invalid numeric configuration");return v;}
bool boolean(const std::string& s) {if(s=="enabled"||s=="true"||s=="1")return true;if(s=="disabled"||s=="false"||s=="0")return false;throw std::runtime_error("Invalid boolean configuration");}
}
int parse(int argc,char** argv) {
 try {
  if(argc!=2)return -1;
  namespace fs=std::filesystem;
  auto base=platf::appdata();fs::create_directories(base);
  fs::permissions(base,fs::perms::owner_all);
  sunshine.log_file=(base/"sunshine.log").string();
  nvhttp.sunshine_name=platf::get_host_name();nvhttp.file_state=(base/"sunshine_state.json").string();
  stream.file_apps=(base/"apps.json").string();
  std::ifstream in(argv[1]);if(!in)throw std::runtime_error("Cannot read configuration");
  std::map<std::string,std::string> values;std::string line;
  while(std::getline(in,line)) {line=trim(line.substr(0,line.find('#')));if(line.empty())continue;auto p=line.find('=');if(p==line.npos)throw std::runtime_error("Invalid configuration line");auto key=trim(line.substr(0,p));if(!values.emplace(key,trim(line.substr(p+1))).second)throw std::runtime_error("Duplicate configuration key");}
  for(auto& [k,v]:values) {
   if(k=="sunshine_name")nvhttp.sunshine_name=v;
   else if(k=="external_ip")nvhttp.external_ip=v;
   else if(k=="address_family") {if(v!="ipv4"&&v!="both")throw std::runtime_error("Invalid address family");sunshine.address_family=v;}
   else if(k=="bind_address")sunshine.bind_address=v;
   else if(k=="port")sunshine.port=number(v,1019,65514);
   else if(k=="min_log_level") {
    const std::map<std::string,int> levels{{"verbose",0},{"debug",1},{"info",2},{"warning",3},{"error",4},{"fatal",5},{"none",6}};
    auto level=levels.find(v);sunshine.min_log_level=level==levels.end()?number(v,0,6):level->second;
   }
   else if(k=="ping_timeout") {auto n=number(v,-1,2147483647);if(n!=-1)stream.ping_timeout=std::chrono::milliseconds(n);}
   else if(k=="fec_percentage")stream.fec_percentage=number(v,1,255);
   else if(k=="packetsize")stream.packetsize=number(v,0,65535);
   else if(k=="lan_encryption_mode")stream.lan_encryption_mode=number(v,0,2);
   else if(k=="wan_encryption_mode")stream.wan_encryption_mode=number(v,0,2);
   else if(k=="stream_audio")audio.stream=boolean(v);
   else if(k=="back_button_timeout")input.back_button_timeout=std::chrono::milliseconds(number(v,-2147483647,2147483647));
   else if(k=="key_repeat_delay") {auto n=number(v,-1,2147483647);if(n>=0)input.key_repeat_delay=std::chrono::milliseconds(n);}
   else if(k=="key_repeat_frequency") {size_t n;double rate=std::stod(v,&n);if(n!=v.size()||!std::isfinite(rate)||rate<0)throw std::runtime_error("Invalid repeat rate");if(rate>0)input.key_repeat_period=std::chrono::duration<double>(1/rate);}
   else if(k=="gamepad") {bool known=false;for(const auto& p:platf::supported_gamepads(nullptr))if(v==p.name)known=true;if(!known)throw std::runtime_error("Invalid gamepad profile");input.gamepad=v;}
   else if(k=="gamepad_driver") {if(v!="all"&&v!="virtualhid"&&v!="vigembus")throw std::runtime_error("Invalid gamepad driver");input.gamepad_driver=v;}
   else if(k=="dd_config_revert_on_disconnect")video.dd.config_revert_on_disconnect=boolean(v);
   else if(k=="keybindings") {
    if(v.size()<2||v.front()!='['||v.back()!=']')throw std::runtime_error("Invalid keybindings");
    std::stringstream list(v.substr(1,v.size()-2));std::string item;std::vector<int> pairs;
    while(std::getline(list,item,',')) {item=trim(item);size_t n;int value=std::stoi(item,&n,0);if(n!=item.size())throw std::runtime_error("Invalid keybinding");pairs.push_back(value);}
    if(pairs.size()%2)throw std::runtime_error("Odd keybinding list");
    for(size_t i=0;i<pairs.size();i+=2)input.keybindings.emplace(pairs[i],pairs[i+1]);
   }
   else if(k=="upnp") {if(boolean(v))throw std::runtime_error("UPnP is unavailable in dedicated host");}
   else if(k=="file_state"||k=="file_apps"||k=="log_path") {auto path=fs::path(v);if(path.is_relative())path=base/path;if(k=="file_state")nvhttp.file_state=path.string();else if(k=="file_apps")stream.file_apps=path.string();else sunshine.log_file=path.string();}
   else {
    static const std::map<std::string,bool input_t::*> booleans{
      {"keyboard",&input_t::keyboard},{"mouse",&input_t::mouse},{"controller",&input_t::controller},
      {"key_rightalt_to_key_win",&input_t::key_rightalt_to_key_win},
      {"always_send_scancodes",&input_t::always_send_scancodes},
      {"high_resolution_scrolling",&input_t::high_resolution_scrolling},{"native_pen_touch",&input_t::native_pen_touch},
      {"ds4_back_as_touchpad_click",&input_t::ds4_back_as_touchpad_click},
      {"motion_as_ds4",&input_t::motion_as_ds4},{"touchpad_as_ds4",&input_t::touchpad_as_ds4},
      {"virtualhid_randomize_mac",&input_t::virtualhid_randomize_mac}};
    auto entry=booleans.find(k);if(entry==booleans.end())throw std::runtime_error("Unsupported dedicated configuration key");
    input.*(entry->second)=boolean(v);
   }
  }
  if(input.key_rightalt_to_key_win)input.keybindings.emplace(0xA5,0x5B);
  if(!fs::exists(stream.file_apps))fs::copy_file("assets/apps.json",stream.file_apps);
  return 0;
 }catch(...) {std::cerr<<"RKMoon configuration refused\n";return -1;}
}
}
