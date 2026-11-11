// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
namespace config {
constexpr int PACKETSIZE_MIN=200,PACKETSIZE_MAX=65535,PACKETSIZE_SMALL=500,PACKETSIZE_LARGE=1456;
constexpr int ENCRYPTION_MODE_NEVER=0,ENCRYPTION_MODE_OPPORTUNISTIC=1,ENCRYPTION_MODE_MANDATORY=2;
inline constexpr std::string_view GAMEPAD_DRIVER_ALL="all",GAMEPAD_DRIVER_VIRTUALHID="virtualhid",GAMEPAD_DRIVER_VIGEMBUS="vigembus";
struct video_t { struct dd_t {bool config_revert_on_disconnect=false;} dd; std::string capture,adapter_name; };
struct audio_t {bool stream=true;};
struct stream_t {std::chrono::milliseconds ping_timeout{10000}; int fec_percentage=20,lan_encryption_mode=0,wan_encryption_mode=1,packetsize=0; std::string file_apps;};
struct nvhttp_t {std::string sunshine_name,file_state,external_ip;};
  struct input_t {
    std::unordered_map<int, int> keybindings;  ///< Client keycode to platform keycode bindings.

    std::chrono::milliseconds back_button_timeout;  ///< Hold duration that turns a controller Back button into a special action.
    std::chrono::milliseconds key_repeat_delay;  ///< Delay before repeating a held keyboard key.
    std::chrono::duration<double> key_repeat_period;  ///< Interval between repeated keyboard key events.

    std::string gamepad;  ///< Virtual controller profile selected by configuration.
    std::string gamepad_driver;  ///< Windows virtual gamepad driver policy, or empty until the user chooses one.
    bool ds4_back_as_touchpad_click;  ///< Map Back/Select to touchpad click for PlayStation-style gamepads.
    bool motion_as_ds4;  ///< Prefer PlayStation-style emulation for client gamepads with motion controls.
    bool touchpad_as_ds4;  ///< Prefer PlayStation-style emulation for client gamepads with touchpad input.
    bool virtualhid_randomize_mac;  ///< Randomize the libvirtualhid virtual controller MAC address.

    bool keyboard;  ///< Enable keyboard input from clients.
    bool key_rightalt_to_key_win;  ///< Map the client Right Alt key to the Windows key.
    bool mouse;  ///< Enable mouse input from clients.
    bool controller;  ///< Enable controller input from clients.

    bool always_send_scancodes;  ///< Always send keyboard scancodes when available.

    bool high_resolution_scrolling;  ///< Enable high-resolution mouse-wheel events.
    bool native_pen_touch;  ///< Enable native pen and touch injection.
  };


struct sunshine_t {int min_log_level=2; uint16_t port=47989; std::string address_family="ipv4",bind_address,log_file;};
extern video_t video;
extern audio_t audio;
extern stream_t stream;
extern nvhttp_t nvhttp;
extern input_t input;
extern sunshine_t sunshine;
int parse(int argc,char** argv);
}
