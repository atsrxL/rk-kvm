// SPDX-License-Identifier: GPL-3.0-or-later
// Six GameStream HTTP routes, extracted from pinned Sunshine 63d35f7.
// No request, header, query, or credential logging.
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include "src/config.h"
#include "src/display_device.h"
#include "src/globals.h"
#include "src/httpcommon.h"
#include "src/logging.h"
#include "src/network.h"
#include "src/nvhttp.h"
#include "src/platform/common.h"
#include "src/process.h"
#include "src/rtsp.h"
#include "src/utility.h"
#include "src/uuid.h"
#include "src/video.h"
#include "rkmoon_bridge.hpp"
#include "rkmoon_display.hpp"
#include "rkmoon_password.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <Simple-Web-Server/server_http.hpp>
#include <filesystem>
#include <fstream>
#include <format>
#include <atomic>
#include <thread>
using namespace std::literals;
namespace http {
std::string unique_id;
net::net_e origin_web_ui_allowed;
int init() {
  try {
    // Keep the existing state schema/identity across upgrades. Never import certs.
    boost::property_tree::ptree tree;
    const auto path = std::filesystem::path(config::nvhttp.file_state);
    if (std::filesystem::exists(path)) boost::property_tree::read_json(path.string(), tree);
    unique_id = tree.get<std::string>("root.uniqueid", "");
    if (unique_id.empty()) {
      unique_id = uuid_util::uuid_t::generate().string();
      tree.put("root.uniqueid", unique_id);
      auto temporary = path; temporary += ".rkmoon-new";
      boost::property_tree::write_json(temporary.string(), tree);
      std::filesystem::permissions(temporary, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
      std::filesystem::rename(temporary, path);
    }
    // Validate even when invoked without the supervisor.
    auto secret = rkmoon_sunshine::read_password_file(std::filesystem::path(std::getenv("HOME")) / "http-password");
    OPENSSL_cleanse(secret.data(), secret.size());
    return 0;
  } catch (...) {
    BOOST_LOG(error) << "RKMoon private HTTP state initialization failed";
    return -1;
  }
}
}
namespace nvhttp {
namespace pt = boost::property_tree;
using args_t = SimpleWeb::CaseInsensitiveMultimap;
using resp_https_t = std::shared_ptr<SimpleWeb::ServerBase<SimpleWeb::HTTP>::Response>;
using req_https_t = std::shared_ptr<SimpleWeb::ServerBase<SimpleWeb::HTTP>::Request>;
std::atomic<uint32_t> session_id_counter {0};
constexpr auto EMPTY_PROPERTY_TREE_ERROR_MSG = "RKMoon HTTP handler returned no status";
  std::string get_arg(const args_t &args, const char *name, const char *default_value = nullptr) {
    auto it = args.find(name);
    if (it == std::end(args)) {
      if (default_value != nullptr) {
        return std::string(default_value);
      }

      throw std::out_of_range(name);
    }
    return it->second;
  }

  std::shared_ptr<rtsp_stream::launch_session_t> make_launch_session(bool host_audio, const args_t &args) {
    auto launch_session = std::make_shared<rtsp_stream::launch_session_t>();

    launch_session->id = ++session_id_counter;

    auto rikey = util::from_hex_vec(get_arg(args, "rikey"), true);
    std::copy(rikey.cbegin(), rikey.cend(), std::back_inserter(launch_session->gcm_key));

    launch_session->host_audio = host_audio;
    std::stringstream mode = std::stringstream(get_arg(args, "mode", "0x0x0"));
    // Split mode by the char "x", to populate width/height/fps
    int x = 0;
    std::string segment;
    while (std::getline(mode, segment, 'x')) {
      if (x == 0) {
        launch_session->width = atoi(segment.c_str());
      }
      if (x == 1) {
        launch_session->height = atoi(segment.c_str());
      }
      if (x == 2) {
        launch_session->fps = atoi(segment.c_str());
      }
      x++;
    }
    launch_session->unique_id = (get_arg(args, "uniqueid", "unknown"));
    launch_session->appid = (int) util::from_view(get_arg(args, "appid", "unknown"));
    launch_session->enable_sops = util::from_view(get_arg(args, "sops", "0"));
    launch_session->surround_info = (int) util::from_view(get_arg(args, "surroundAudioInfo", "196610"));
    launch_session->surround_params = (get_arg(args, "surroundParams", ""));
    launch_session->continuous_audio = util::from_view(get_arg(args, "continuousAudio", "0"));
    launch_session->gcmap = (int) util::from_view(get_arg(args, "gcmap", "0"));
    launch_session->enable_hdr = util::from_view(get_arg(args, "hdrMode", "0"));

    // Encrypted RTSP is enabled with client reported corever >= 1
    auto corever = util::from_view(get_arg(args, "corever", "0"));
    if (corever >= 1) {
      launch_session->rtsp_cipher = crypto::cipher::gcm_t {
        launch_session->gcm_key,
        false
      };
      launch_session->rtsp_iv_counter = 0;
    }
    launch_session->rtsp_url_scheme = launch_session->rtsp_cipher ? "rtspenc://"s : "rtsp://"s;
    launch_session->client_cert.clear();
    launch_session->client_name.clear();

    // Generate the unique identifiers for this connection that we will send later during RTSP handshake
    unsigned char raw_payload[8];
    RAND_bytes(raw_payload, sizeof(raw_payload));
    launch_session->av_ping_payload = util::hex_vec(raw_payload);
    RAND_bytes((unsigned char *) &launch_session->control_connect_data, sizeof(launch_session->control_connect_data));

    launch_session->iv.resize(16);
    uint32_t prepend_iv = util::endian::big<uint32_t>((int) util::from_view(get_arg(args, "rikeyid")));
    auto prepend_iv_p = (uint8_t *) &prepend_iv;
    std::copy(prepend_iv_p, prepend_iv_p + sizeof(prepend_iv), std::begin(launch_session->iv));
    return launch_session;
  }

  uint32_t get_codec_mode_flags() {
    uint32_t codec_mode_flags = SCM_H264;
    if (video::last_encoder_probe_supported_yuv444_for_codec[0]) {
      codec_mode_flags |= SCM_H264_HIGH8_444;
    }
    if (video::active_hevc_mode >= 2) {
      codec_mode_flags |= SCM_HEVC;
      if (video::last_encoder_probe_supported_yuv444_for_codec[1]) {
        codec_mode_flags |= SCM_HEVC_REXT8_444;
      }
    }
    if (video::active_hevc_mode == 3 || video::active_hevc_mode == 5) {
      codec_mode_flags |= SCM_HEVC_MAIN10;
    }
    if ((video::active_hevc_mode == 4 || video::active_hevc_mode == 5) && video::last_encoder_probe_supported_yuv444_for_codec[1]) {
      codec_mode_flags |= SCM_HEVC_REXT10_444;
    }

    if (video::active_av1_mode >= 2) {
      codec_mode_flags |= SCM_AV1_MAIN8;
      if (video::last_encoder_probe_supported_yuv444_for_codec[2]) {
        codec_mode_flags |= SCM_AV1_HIGH8_444;
      }
    }
    if (video::active_av1_mode == 3 || video::active_av1_mode == 5) {
      codec_mode_flags |= SCM_AV1_MAIN10;
    }
    if ((video::active_av1_mode == 4 || video::active_av1_mode == 5) && video::last_encoder_probe_supported_yuv444_for_codec[2]) {
      codec_mode_flags |= SCM_AV1_HIGH10_444;
    }
    return codec_mode_flags;
  }

  template<class T>
  void serverinfo(std::shared_ptr<typename SimpleWeb::ServerBase<T>::Response> response, std::shared_ptr<typename SimpleWeb::ServerBase<T>::Request> request) {

    auto local_endpoint = request->local_endpoint();

    pt::ptree tree;

    tree.put("root.<xmlattr>.status_code", 200);
    tree.put("root.hostname", config::nvhttp.sunshine_name);

    tree.put("root.appversion", VERSION);
    tree.put("root.GfeVersion", GFE_VERSION);
    tree.put("root.uniqueid", http::unique_id);
    tree.put("root.HttpsPort", 0);
    tree.put("root.ExternalPort", net::map_port(PORT_HTTP));
    tree.put("root.MaxLumaPixelsHEVC", video::active_hevc_mode > 1 ? "1869449984" : "0");

    tree.put("root.mac", "00:00:00:00:00:00");

    // Moonlight clients track LAN IPv6 addresses separately from LocalIP which is expected to
    // always be an IPv4 address. If we return that same IPv6 address here, it will clobber the
    // stored LAN IPv4 address. To avoid this, we need to return an IPv4 address in this field
    // when we get a request over IPv6.
    //
    // HACK: We should return the IPv4 address of local interface here, but we don't currently
    // have that implemented. For now, we will emulate the behavior of GFE+GS-IPv6-Forwarder,
    // which returns 127.0.0.1 as LocalIP for IPv6 connections. Moonlight clients with IPv6
    // support know to ignore this bogus address.
    if (local_endpoint.address().is_v6() && !local_endpoint.address().to_v6().is_v4_mapped()) {
      tree.put("root.LocalIP", "127.0.0.1");
    } else {
      tree.put("root.LocalIP", net::addr_to_normalized_string(local_endpoint.address()));
    }

    const uint32_t codec_mode_flags = get_codec_mode_flags();
    tree.put("root.ServerCodecModeSupport", codec_mode_flags);

    if (!config::nvhttp.external_ip.empty()) {
      tree.put("root.ExternalIP", config::nvhttp.external_ip);
    }

    auto current_appid = proc::proc.running();
    rkmoon_sunshine::put_display_info(tree);
    tree.put("root.RKMoonAuth", "password-http-v1");
    tree.put("root.PairStatus", 1);
    tree.put("root.currentgame", current_appid);
    tree.put("root.state", current_appid > 0 ? "SUNSHINE_SERVER_BUSY" : "SUNSHINE_SERVER_FREE");

    std::ostringstream data;

    pt::write_xml(data, tree);
    response->write(data.str());
    response->close_connection_after_response = true;
  }

  void applist(resp_https_t response, req_https_t request) {

    pt::ptree tree;

    auto g = util::fail_guard([&]() {
      std::ostringstream data;

      pt::write_xml(data, tree);
      response->write(data.str());
      response->close_connection_after_response = true;
    });

    auto &apps = tree.add_child("root", pt::ptree {});

    apps.put("<xmlattr>.status_code", 200);

    for (auto &proc : proc::proc.get_apps()) {
      pt::ptree app;

      app.put("IsHdrSupported"s, video::active_hevc_mode >= 3 ? 1 : 0);
      app.put("AppTitle"s, proc.name);
      app.put("ID", proc.id);

      apps.push_back(std::make_pair("App", std::move(app)));
    }
  }

  void launch(bool &host_audio, resp_https_t response, req_https_t request) {

    pt::ptree tree;
    bool revert_display_configuration {false};
    auto g = util::fail_guard([&]() {
      std::ostringstream data;

      if (tree.empty()) {
        BOOST_LOG(error) << EMPTY_PROPERTY_TREE_ERROR_MSG;
      }

      pt::write_xml(data, tree);
      response->write(data.str());
      response->close_connection_after_response = true;

      if (revert_display_configuration) {
        display_device::revert_configuration();
      }
    });

    auto args = request->parse_query_string();
    if (
      args.find("rikey"s) == std::end(args) ||
      args.find("rikeyid"s) == std::end(args) ||
      args.find("localAudioPlayMode"s) == std::end(args) ||
      args.find("appid"s) == std::end(args)
    ) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 400);
      tree.put("root.<xmlattr>.status_message", "Missing a required launch parameter");

      return;
    }

    auto appid = util::from_view(get_arg(args, "appid"));

    auto current_appid = proc::proc.running();
    if (current_appid > 0) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 400);
      tree.put("root.<xmlattr>.status_message", "An app is already running on this host");

      return;
    }

    host_audio = util::from_view(get_arg(args, "localAudioPlayMode"));
    const auto rkmoon_mode = rkmoon_sunshine::mouse_mode(args);
    if (!rkmoon_mode.has_value()) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 400);
      tree.put("root.<xmlattr>.status_message", "Unsupported RKMoon mouse mode");
      return;
    }
    auto launch_session = make_launch_session(host_audio, args);
    launch_session->rkmoon_absolute_mouse = *rkmoon_mode;

    if (rtsp_stream::session_count() == 0) {
      // The display should be restored in case something fails as there are no other sessions.
      revert_display_configuration = true;

      // We want to prepare display only if there are no active sessions at
      // the moment. This should be done before probing encoders as it could
      // change the active displays.
      display_device::configure_display(config::video, *launch_session);

      // Probe encoders again before streaming to ensure our chosen
      // encoder matches the active GPU (which could have changed
      // due to hotplugging, driver crash, primary monitor change,
      // or any number of other factors).
      if (video::probe_encoders()) {
        tree.put("root.<xmlattr>.status_code", 503);
        tree.put("root.<xmlattr>.status_message", "Failed to initialize video capture/encoding. Is a display connected and turned on?");
        tree.put("root.gamesession", 0);

        return;
      }
    }

    auto encryption_mode = net::encryption_mode_for_address(request->remote_endpoint().address());
    if (!launch_session->rtsp_cipher && encryption_mode == config::ENCRYPTION_MODE_MANDATORY) {
      BOOST_LOG(error) << "Rejecting client that cannot comply with mandatory encryption requirement"sv;

      tree.put("root.<xmlattr>.status_code", 403);
      tree.put("root.<xmlattr>.status_message", "Encryption is mandatory for this host but unsupported by the client");
      tree.put("root.gamesession", 0);

      return;
    }

    if (appid > 0) {
      auto err = proc::proc.execute((int) appid, launch_session);
      if (err) {
        tree.put("root.<xmlattr>.status_code", err);
        tree.put("root.<xmlattr>.status_message", "Failed to start the specified application");
        tree.put("root.gamesession", 0);

        return;
      }
    }

    tree.put("root.<xmlattr>.status_code", 200);
    tree.put(
      "root.sessionUrl0",
      std::format(
        "{}{}:{}",
        launch_session->rtsp_url_scheme,
        net::addr_to_url_escaped_string(request->local_endpoint().address()),
        static_cast<int>(net::map_port(rtsp_stream::RTSP_SETUP_PORT))
      )
    );
    tree.put("root.gamesession", 1);

    rtsp_stream::launch_session_raise(launch_session);

    // Stream was started successfully, we will revert the config when the app or session terminates
    revert_display_configuration = false;
  }

  void resume(bool &host_audio, resp_https_t response, req_https_t request) {

    pt::ptree tree;
    auto g = util::fail_guard([&]() {
      std::ostringstream data;

      if (tree.empty()) {
        BOOST_LOG(error) << EMPTY_PROPERTY_TREE_ERROR_MSG;
      }

      pt::write_xml(data, tree);
      response->write(data.str());
      response->close_connection_after_response = true;
    });

    auto current_appid = proc::proc.running();
    if (current_appid == 0) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 503);
      tree.put("root.<xmlattr>.status_message", "No running app to resume");

      return;
    }

    auto args = request->parse_query_string();
    if (
      args.find("rikey"s) == std::end(args) ||
      args.find("rikeyid"s) == std::end(args)
    ) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 400);
      tree.put("root.<xmlattr>.status_message", "Missing a required resume parameter");

      return;
    }

    // Newer Moonlight clients send localAudioPlayMode on /resume too,
    // so we should use it if it's present in the args and there are
    // no active sessions we could be interfering with.
    const bool no_active_sessions {rtsp_stream::session_count() == 0};
    if (no_active_sessions && args.find("localAudioPlayMode"s) != std::end(args)) {
      host_audio = util::from_view(get_arg(args, "localAudioPlayMode"));
    }
    const auto rkmoon_mode = rkmoon_sunshine::mouse_mode(args);
    if (!rkmoon_mode.has_value()) {
      tree.put("root.resume", 0);
      tree.put("root.<xmlattr>.status_code", 400);
      tree.put("root.<xmlattr>.status_message", "Unsupported RKMoon mouse mode");
      return;
    }
    const auto launch_session = make_launch_session(host_audio, args);
    launch_session->rkmoon_absolute_mouse = *rkmoon_mode;

    if (no_active_sessions) {
      // We want to prepare display only if there are no active sessions at
      // the moment. This should be done before probing encoders as it could
      // change the active displays.
      display_device::configure_display(config::video, *launch_session);

      // Probe encoders again before streaming to ensure our chosen
      // encoder matches the active GPU (which could have changed
      // due to hotplugging, driver crash, primary monitor change,
      // or any number of other factors).
      if (video::probe_encoders()) {
        tree.put("root.resume", 0);
        tree.put("root.<xmlattr>.status_code", 503);
        tree.put("root.<xmlattr>.status_message", "Failed to initialize video capture/encoding. Is a display connected and turned on?");

        return;
      }
    }

    auto encryption_mode = net::encryption_mode_for_address(request->remote_endpoint().address());
    if (!launch_session->rtsp_cipher && encryption_mode == config::ENCRYPTION_MODE_MANDATORY) {
      BOOST_LOG(error) << "Rejecting client that cannot comply with mandatory encryption requirement"sv;

      tree.put("root.<xmlattr>.status_code", 403);
      tree.put("root.<xmlattr>.status_message", "Encryption is mandatory for this host but unsupported by the client");
      tree.put("root.gamesession", 0);

      return;
    }

    tree.put("root.<xmlattr>.status_code", 200);
    tree.put(
      "root.sessionUrl0",
      std::format(
        "{}{}:{}",
        launch_session->rtsp_url_scheme,
        net::addr_to_url_escaped_string(request->local_endpoint().address()),
        static_cast<int>(net::map_port(rtsp_stream::RTSP_SETUP_PORT))
      )
    );
    tree.put("root.resume", 1);

    rtsp_stream::launch_session_raise(launch_session);
  }

  void cancel(resp_https_t response, req_https_t request) {

    pt::ptree tree;
    auto g = util::fail_guard([&]() {
      std::ostringstream data;

      pt::write_xml(data, tree);
      response->write(data.str());
      response->close_connection_after_response = true;
    });

    tree.put("root.cancel", 1);
    tree.put("root.<xmlattr>.status_code", 200);

    rtsp_stream::terminate_sessions();

    if (proc::proc.running() > 0) {
      proc::proc.terminate();
    }

    // The config needs to be reverted regardless of whether "proc::proc.terminate()" was called or not.
    display_device::revert_configuration();
  }

  void appasset(resp_https_t response, req_https_t request) {

    auto args = request->parse_query_string();
    auto app_image = proc::proc.get_app_image((int) util::from_view(get_arg(args, "appid")));

    std::ifstream in(app_image, std::ios::binary);
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "image/png");
    response->write(SimpleWeb::StatusCode::success_ok, in, headers);
    response->close_connection_after_response = true;
  }

  void start() {
    platf::set_thread_name("nvhttp");
    auto shutdown_event = mail::man->event<bool>(mail::shutdown);

    auto port_http = net::map_port(PORT_HTTP);
    auto address_family = net::af_from_enum_string(config::sunshine.address_family);

    // resume doesn't always get the parameter "localAudioPlayMode"
    // launch will store it in host_audio
    bool host_audio {};

    SimpleWeb::Server<SimpleWeb::HTTP> http_server;

    http_server.default_resource["GET"] = [](auto response, auto) { response->write("<root status_code=\"404\"/>"); response->close_connection_after_response = true; };
    http_server.resource["^/serverinfo$"]["GET"] = serverinfo<SimpleWeb::HTTP>;

    http_server.resource["^/applist$"]["GET"] = applist;
    http_server.resource["^/appasset$"]["GET"] = appasset;
    http_server.resource["^/launch$"]["GET"] = [&host_audio](auto resp, auto req) {
      launch(host_audio, resp, req);
    };
    http_server.resource["^/resume$"]["GET"] = [&host_audio](auto resp, auto req) {
      resume(host_audio, resp, req);
    };
    http_server.resource["^/cancel$"]["GET"] = cancel;

    // Authenticate every HTTP request, including keep-alive requests.
    rkmoon_sunshine::password_auth password;
    const auto password_path = std::filesystem::path(std::getenv("HOME")) / "http-password";
    for (auto &[path, methods] : http_server.resource) {
      for (auto &[method, handler] : methods) {
        handler = [&password, &password_path, next = handler](auto response, auto request) {
          const auto range = request->header.equal_range("Authorization");
          const bool single = range.first != range.second && std::next(range.first) == range.second;
          if (!password.authorize_file(password_path, single ? std::string_view(range.first->second) : std::string_view{})) {
            response->write("<root status_code=\"401\" status_message=\"Password authentication failed\"/>");
            response->close_connection_after_response = true;
            return;
          }
          try { next(response, request); } catch (...) {
            response->write("<root status_code=\"400\" status_message=\"Invalid request\"/>");
            response->close_connection_after_response = true;
          }
        };
      }
    }



    http_server.config.reuse_address = true;
    http_server.config.address = net::get_bind_address(address_family);
    http_server.config.port = port_http;

    auto accept_and_run = [&](auto *http_server) {
      try {
        std::string name = "nvhttp::" + std::to_string(http_server->config.port);
        platf::set_thread_name(name);
        http_server->start();
      } catch (boost::system::system_error &err) {
        // It's possible the exception gets thrown after calling http_server->stop() from a different thread
        if (shutdown_event->peek()) {
          return;
        }

        BOOST_LOG(fatal) << "Couldn't start http server on ports ["sv << port_http << "]: "sv << err.what();
        shutdown_event->raise(true);
        return;
      }
    };
    std::jthread tcp {accept_and_run, &http_server};

    // Wait for any event
    shutdown_event->view();

    http_server.stop();

    tcp.join();
  }

}
