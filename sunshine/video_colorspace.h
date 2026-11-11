// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace video {
enum class colorspace_e {rec601,rec709,bt2020sdr,bt2020};
struct sunshine_colorspace_t {colorspace_e colorspace; bool full_range; unsigned bit_depth;};
}
