#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Commit-locked, fail-closed transformations for the copied protocol subset."""
import re
from pathlib import Path
import subprocess

PIN="63d35f702ee9e362e43263742981836ec0710384"
ROOT=Path(__file__).resolve().parents[1]

class PatchError(RuntimeError):
    pass

def once(text, old, new):
    count=text.count(old)
    if count != 1:
        raise PatchError(f"anchor count {count}, expected 1: {old[:100]!r}")
    return text.replace(old,new,1)

def at_entry(text, signature, body):
    # Permit upstream's multiline formatting without guessing a different symbol/overload.
    tokens=re.findall(r"\w+|::|&&|[^\w\s]",signature)
    pattern=r"\s*".join(re.escape(t) for t in tokens)+r"\s*\{[ \t]*\n"
    matches=list(re.finditer(pattern,text))
    if len(matches)!=1:
        raise PatchError(f"function anchor count {len(matches)}, expected 1: {signature}")
    end=matches[0].end()
    return text[:end]+body+text[end:]

def make_changes(original):
    changes={}
    for path,text in original.items():
        if path not in {"src/logging.cpp", "src/thread_safe.h", "src/rtsp.h"}:
            text=once(text,"// local includes\n",'// local includes\n#include "src/rkmoon/rkmoon_bridge.hpp"\n')
        if path=="src/rtsp.h":
            anchor="    std::string client_cert;  ///< PEM certificate for the paired Moonlight client.\n"
            text=once(text,anchor,anchor+"    bool rkmoon_absolute_mouse = false;\n")
        elif path=="src/input.cpp":
            text=once(text,"    task_pool.pushDelayed([]() {\n      platf::move_mouse(platf_input, 1, 1);","    task_pool.pushDelayed([]() {\n      if (rkmoon_sunshine::enabled()) return; // HDMI needs no desktop cursor wake; preserve selected USB mode.\n      platf::move_mouse(platf_input, 1, 1);")
            text=at_entry(text,"  void print(void *payload)",
                "    if (rkmoon_sunshine::enabled()) { return; } // Never log decoded input, even at verbose.\n")
            text=at_entry(text,"  inline int apply_shortcut(short keyCode)","    if (rkmoon_sunshine::enabled()) { return 0; } // Send shortcuts to the USB host, not T6.\n")
        elif path=="src/logging.cpp":
            text=once(text, '#else\n  #include <display_device/logging.h>\n', '')
            text=once(text, 'extern "C" {\n#include <libavutil/log.h>\n}\n', '')
            text=once(text, '#ifndef __ANDROID__\n    setup_av_logging(min_log_level);\n    setup_libdisplaydevice_logging(min_log_level);\n#endif\n', '')
            begin=text.index('#ifndef __ANDROID__\n  void setup_av_logging(')
            end=text.index('\n  void log_flush()',begin)
            removed=text[begin:end]
            if removed.count('setup_av_logging(')!=1 or removed.count('setup_libdisplaydevice_logging(')!=1:
                raise PatchError('logging integration anchor count changed')
            text=once(text,removed,'')
        elif path=="src/stream.cpp":
            text=once(text,'        << util::hex_vec(payload) << std::endl',
                '        << "[RKMoon control payload redacted]" << std::endl')
            for kind in ('ping [v2]', 'ping [v1]', 'non-ping'):
                old='        BOOST_LOG(debug) << "Received '+kind+' from "sv << recv_peer.address() << \':\' << recv_peer.port() << " ["sv << util::hex_vec(msg) << \']\';'
                new='        BOOST_LOG(debug) << "Received '+kind+' (payload redacted)";'
                text=once(text,old,new)
        elif path=="src/rtsp.cpp":
            # Keep sizes/status but never log SDP/options/raw request or response.
            text=at_entry(text,"  void print_msg(PRTSP_MESSAGE msg)",
                '    if (rkmoon_sunshine::enabled()) { BOOST_LOG(debug) << "RKMoon RTSP message (content redacted)"; return; }\n')
            text=once(text,'          BOOST_LOG(debug) << "Found Host: "sv << content;',
                '          BOOST_LOG(debug) << "Found RTSP Host header (redacted)";')
            anchor="    auto stream_session = stream::session::alloc(config, session);\n"
            text=once(text,anchor,"""    // RKMoon admission occurs AFTER upstream parsing/encryption validation and BEFORE input allocation.
    if (rkmoon_sunshine::enabled()) {
      if (server->session_count() != 0) {
        respond(sock, session, &option, 453, "RKMoon exclusive session busy", req->sequenceNumber, {});
        return;
      }
      config.monitor.rkmoon_absolute_mouse = session.rkmoon_absolute_mouse;
      if (!rkmoon_sunshine::request_supported(config.monitor)) {
        respond(sock, session, &option, 400, "RKMoon unsupported mode or capture not authorized", req->sequenceNumber, {});
        return;
      }
    }
"""+anchor)
        elif path=="src/thread_safe.h":
            anchor="    explicit queue_t(std::uint32_t max_elements = 32, overflow_policy_e overflow = overflow_policy_e::drop_oldest):"
            methods="""    // RKMoon producer-specific methods: bounded, locked, no implicit reference-frame loss.
    bool rkmoon_try_raise(std::size_t limit, T &&item) {
      std::lock_guard lock {_lock};
      if (!_continue || _queue.size() >= limit) { return false; }
      _queue.emplace_back(std::move(item));
      _cv.notify_all();
      return true;
    }
    std::size_t rkmoon_size() { std::lock_guard lock {_lock}; return _queue.size(); }
    bool rkmoon_running() { std::lock_guard lock {_lock}; return _continue; }

"""
            text=once(text,anchor,methods+anchor)
        else:
            raise PatchError("unexpected source file")
        changes[path]=text
    return changes

def git(repo,*args):
    return subprocess.check_output(["git","-C",str(repo),*args],text=True).strip()

if __name__=="__main__":
    raise SystemExit("Use tools/prepare_sunshine.py to copy the pinned subset; in-place overlays are retired")
