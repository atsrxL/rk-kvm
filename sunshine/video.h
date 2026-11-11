// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "platform/common.h"
#include "thread_safe.h"
#include <array>
#include <chrono>
#include <optional>
namespace video {
  struct config_t {
    int width;  ///< Video width in pixels.
    int height;  ///< Video height in pixels.
    int framerate;  ///< Requested framerate used in the per-frame bitrate budget.
    int framerateX100;  ///< Optional NTSC-style framerate value, e.g. 59.94 as 5994.
    int bitrate;  ///< Video bitrate in kilobits for the requested framerate.
    int slicesPerFrame;  ///< Number of slices per frame.
    int numRefFrames;  ///< Maximum number of reference frames.
    int encoderCscMode;  ///< Requested color range and SDR colorspace; HDR always uses BT.2020 and ST2084.
    int videoFormat;  ///< Video codec format: 0 = H.264, 1 = HEVC, 2 = AV1.
    int dynamicRange;  ///< Encoding color depth: 0 = 8-bit, 1 = 10-bit.
    int chromaSamplingType;  ///< Chroma sampling type: 0 = 4:2:0, 1 = 4:4:4.
    int enableIntraRefresh;  ///< Intra refresh setting: 0 = disabled, 1 = enabled.
    bool rkmoon_absolute_mouse = false;
  };
  struct packet_raw_t {
    virtual ~packet_raw_t() = default;

    /**
     * @brief Report whether this packet starts an IDR frame.
     *
     * @return True when this frame is an IDR frame.
     */
    virtual bool is_idr() = 0;

    /**
     * @brief Return the frame index associated with this encoded packet.
     *
     * @return Monotonic frame index assigned to this frame.
     */
    virtual int64_t frame_index() = 0;

    /**
     * @brief Return writable access to the encoded packet bytes.
     *
     * @return Pointer to the first byte of encoded frame data.
     */
    virtual uint8_t *data() = 0;

    /**
     * @brief Return the encoded payload length.
     *
     * @return Size of the encoded frame payload in bytes.
     */
    virtual size_t data_size() = 0;

    /**
     * @brief Packet byte-range replacement descriptor.
     */
    struct replace_t {
      std::string_view old;  ///< Byte sequence to find in the encoded packet.
      std::string_view _new;  ///< Byte sequence that replaces `old` before transmission.

      KITTY_DEFAULT_CONSTR_MOVE(replace_t)

      /**
       * @brief Store an encoded-byte replacement rule.
       *
       * @param old Byte sequence to replace in encoded packets.
       * @param _new Replacement byte sequence inserted into encoded packets.
       */
      replace_t(std::string_view old, std::string_view _new) noexcept:
          old {std::move(old)},
          _new {std::move(_new)} {
      }
    };

    std::vector<replace_t> *replacements = nullptr;  ///< Optional encoded-byte substitutions applied before packetization.
    void *channel_data = nullptr;  ///< Platform or protocol state carried with this packet.
    bool after_ref_frame_invalidation = false;  ///< Whether the frame follows reference-frame invalidation.
    std::optional<std::chrono::steady_clock::time_point> frame_timestamp;  ///< Capture timestamp associated with the frame.
  };

  struct packet_raw_generic: packet_raw_t {
    /**
     * @brief Wrap generic encoded frame bytes in Sunshine packet metadata.
     *
     * @param frame_data Encoded frame bytes produced by a non-FFmpeg encoder.
     * @param frame_index Monotonic frame index assigned to the encoded frame.
     * @param idr Whether the packet begins an IDR frame.
     */
    packet_raw_generic(std::vector<uint8_t> &&frame_data, int64_t frame_index, bool idr):
        frame_data {std::move(frame_data)},
        index {frame_index},
        idr {idr} {
    }

    /**
     * @brief Report whether the generic packet starts an IDR frame.
     *
     * @return True when this frame is an IDR frame.
     */
    bool is_idr() override {
      return idr;
    }

    /**
     * @brief Return the stored frame index for this generic packet.
     *
     * @return Monotonic frame index assigned to this frame.
     */
    int64_t frame_index() override {
      return index;
    }

    /**
     * @brief Return writable access to the generic packet payload.
     *
     * @return Pointer to the first encoded byte in `frame_data`.
     */
    uint8_t *data() override {
      return frame_data.data();
    }

    /**
     * @brief Return the generic packet payload length.
     *
     * @return Size of the encoded frame payload in bytes.
     */
    size_t data_size() override {
      return frame_data.size();
    }

    std::vector<uint8_t> frame_data;  ///< Encoded frame bytes owned by this packet.
    int64_t index;  ///< Monotonic frame index assigned to this packet.
    bool idr;  ///< Whether the packet belongs to an IDR frame.
  };

  /**
   * @brief Owning pointer to an encoded packet abstraction.
   */
  using packet_t = std::unique_ptr<packet_raw_t>;

  /**
   * @brief Raw HDR metadata fields parsed from the display backend.
   */
  struct hdr_info_raw_t {
    /**
     * @brief Initialize HDR metadata with only the enabled state.
     *
     * @param enabled Whether the feature should be enabled.
     */
    explicit hdr_info_raw_t(bool enabled):
        enabled {enabled},
        metadata {} {};
    /**
     * @brief Initialize HDR metadata with display metadata from the backend.
     *
     * @param enabled Whether the feature should be enabled.
     * @param metadata Output structure populated with HDR metadata.
     */
    explicit hdr_info_raw_t(bool enabled, const SS_HDR_METADATA &metadata):
        enabled {enabled},
        metadata {metadata} {};

    bool enabled;  ///< Whether HDR mode should be enabled.
    SS_HDR_METADATA metadata;  ///< Display HDR metadata forwarded to the encoder and client.
  };

  /**
   * @brief Owning pointer to optional HDR metadata for a captured display.
   */
  using hdr_info_t = std::unique_ptr<hdr_info_raw_t>;

  extern int active_hevc_mode;
  extern int active_av1_mode;
  extern bool last_encoder_probe_supported_ref_frames_invalidation;
  extern std::array<bool, 3> last_encoder_probe_supported_yuv444_for_codec;  // 0 - H.264, 1 - HEVC, 2 - AV1


void capture(safe::mail_t,config_t,void*);
int probe_encoders();
}
