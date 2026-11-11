// SPDX-License-Identifier: GPL-3.0-or-later
#include "rkmoon_bridge.hpp"
#include "src/config.h"
#include "src/process.h"
#include "src/display_device.h"
#include "src/system_tray.h"
#include "src/globals.h"
#include "src/stat_trackers.h"
namespace video {
int active_hevc_mode=1,active_av1_mode=1;
bool last_encoder_probe_supported_ref_frames_invalidation=false;
std::array<bool,3> last_encoder_probe_supported_yuv444_for_codec{};
int probe_encoders() {return rkmoon_sunshine::probe();}
void capture(safe::mail_t m,config_t c,void* p) {rkmoon_sunshine::capture(std::move(m),c,p);}
}
namespace audio {
constexpr auto SAMPLE_RATE=48000;
  opus_stream_config_t stream_configs[MAX_STREAM_CONFIG] {
    {
      SAMPLE_RATE,
      2,
      1,
      1,
      platf::speaker::map_stereo.data(),
      96000,
    },
    {
      SAMPLE_RATE,
      2,
      1,
      1,
      platf::speaker::map_stereo.data(),
      512000,
    },
    {
      SAMPLE_RATE,
      6,
      4,
      2,
      platf::speaker::map_surround51.data(),
      256000,
    },
    {
      SAMPLE_RATE,
      6,
      6,
      0,
      platf::speaker::map_surround51.data(),
      1536000,
    },
    {
      SAMPLE_RATE,
      8,
      5,
      3,
      platf::speaker::map_surround71.data(),
      450000,
    },
    {
      SAMPLE_RATE,
      8,
      8,
      0,
      platf::speaker::map_surround71.data(),
      2048000,
    },
  };
void capture(safe::mail_t m,config_t c,void* p) {rkmoon_sunshine::audio_capture(std::move(m),c,p);}
}
namespace proc {
proc_t proc;
int proc_t::execute(int id,std::shared_ptr<rtsp_stream::launch_session_t>) {if(id!=1)return 404;int expected=0;return active.compare_exchange_strong(expected,id)?0:400;}
std::unique_ptr<deinit_t> init() {return std::make_unique<deinit_t>();}
}
namespace display_device {void revert_configuration() {} void configure_display(const config::video_t&,const rtsp_stream::launch_session_t&) {}}
namespace system_tray {void update_tray_playing(std::string) {} void update_tray_pausing(std::string) {}}
namespace lifetime {
char** argv=nullptr;std::atomic_int desired_exit_code{0};
char** get_argv() {return argv;}
void exit_sunshine(int code,bool) {desired_exit_code=code;mail::man->event<bool>(mail::shutdown)->raise(true);}
void debug_trap() {std::abort();}
}
namespace stat_trackers {
boost::format one_digit_after_decimal() {return boost::format("%1$.1f");}
boost::format two_digits_after_decimal() {return boost::format("%1$.2f");}
}
