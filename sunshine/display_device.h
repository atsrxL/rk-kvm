// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "config.h"
#include "rtsp.h"
namespace display_device {void revert_configuration();void configure_display(const config::video_t&,const rtsp_stream::launch_session_t&);}
