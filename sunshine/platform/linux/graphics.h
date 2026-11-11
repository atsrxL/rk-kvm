// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Legacy desktop initialization is unreachable behind the dedicated init guard.
// Fail closed if that guard is ever removed; never load a graphics library.
namespace gbm {inline int init() {return -1;}}
inline int gladLoaderLoadEGL(void*) {return 0;}
