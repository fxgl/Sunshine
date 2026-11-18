/**
 * @file src/stream.h
 * @brief Declarations for the streaming protocols.
 */
#pragma once

// standard includes
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// lib includes
#include <boost/asio.hpp>

// local includes
#include "audio.h"
#include "crypto.h"
#include "video.h"

namespace rtsp_stream {
  struct launch_session_t;
}

namespace stream {
  constexpr auto VIDEO_STREAM_PORT = 9;  ///< GameStream base-port offset used for the video UDP stream.
  constexpr auto CONTROL_PORT = 10;  ///< GameStream base-port offset used for the control channel.
  constexpr auto AUDIO_STREAM_PORT = 11;  ///< GameStream base-port offset used for the audio UDP stream.

  struct session_t;

  /**
   * @brief Stream configuration shared by capture and network senders.
   */
  struct config_t {
    audio::config_t audio;  ///< Audio capture configuration for the stream.
    video::config_t monitor;  ///< Video capture and encoder configuration for the selected monitor.

    int configuredBitrateKbps;  ///< Client-selected total bitrate ceiling before protocol overhead adjustments.
    int packetsize;  ///< Maximum payload size for network packets.
    int minRequiredFecPackets;  ///< Minimum recovery packets required before FEC is emitted.
    int mlFeatureFlags;  ///< Moonlight feature flags negotiated for this session.
    int controlProtocolType;  ///< GameStream control protocol variant selected by the client.
    int audioQosType;  ///< Audio QoS type.
    int videoQosType;  ///< Video QoS type.

    uint32_t encryptionFlagsEnabled;  ///< Bitmask of GameStream encryption features enabled for the session.

    std::optional<int> gcmap;  ///< Optional game-controller mapping override from the launch request.
  };

  /**
   * @brief Validate and convert a Moonlight adaptive-quality control payload.
   * @param config Original negotiated stream configuration and quality ceilings.
   * @param payload Raw SS_ADAPTIVE_STREAM_CONFIG payload.
   * @return Updated encoder configuration, or std::nullopt when the request is invalid.
   */
  std::optional<video::config_t> make_adaptive_video_config(const config_t &config, std::string_view payload);

  /** Encode the host display list for Moonlight's control-channel extension. */
  std::string make_host_display_list_payload(const std::vector<std::string> &display_names, int current_display);

  /** Validate and decode a requested host display index. */
  std::optional<std::uint16_t> parse_host_display_switch(std::string_view payload, std::size_t display_count);

  /**
   * @brief Validate and decode a video pause state payload.
   *
   * @param payload Raw one-byte SS_VIDEO_PAUSE_PTYPE payload.
   * @return Pause state, or std::nullopt when the payload is malformed.
   */
  std::optional<bool> parse_video_pause(std::string_view payload);

  /**
   * @brief One chunk of a clipboard image transfer.
   */
  struct clipboard_image_chunk_t {
    std::uint32_t transfer_id;  ///< Identifies the transfer the chunk belongs to.
    std::uint32_t total_length;  ///< Size of the complete PNG in bytes.
    std::uint32_t offset;  ///< Position of this chunk within the PNG.
    std::string_view data;  ///< Chunk bytes; points into the parsed packet.
  };

  /**
   * @brief Validate and decode an SS_CLIPBOARD_IMAGE_CHUNK_EVENT_MAGIC input packet.
   *
   * @param packet Complete input packet including its NV_INPUT_HEADER.
   * @return Decoded chunk, or std::nullopt when the packet is malformed.
   */
  std::optional<clipboard_image_chunk_t> parse_clipboard_image_chunk(std::string_view packet);

  /**
   * @brief Reassembles chunked clipboard images received from Moonlight.
   *
   * Chunks must arrive in order. A chunk at offset 0 starts a new transfer and
   * discards any partial one; chunks of other transfers are ignored, and a gap
   * within the current transfer discards it.
   */
  class clipboard_image_assembler_t {
  public:
    /**
     * @brief Add one chunk.
     *
     * @param chunk Validated chunk.
     * @return Complete PNG when this chunk finishes the transfer.
     */
    std::optional<std::string> add(const clipboard_image_chunk_t &chunk);

  private:
    std::uint32_t _transfer_id {};  ///< Transfer being reassembled.
    std::uint32_t _total_length {};  ///< Expected PNG size.
    std::string _data;  ///< Bytes received so far; empty when idle.
  };

  namespace session {
    /**
     * @brief Enumerates supported state options.
     */
    enum class state_e : int {
      STOPPED,  ///< The session is stopped
      STOPPING,  ///< The session is stopping
      STARTING,  ///< The session is starting
      RUNNING,  ///< The session is running
    };

    /**
     * @brief Allocate and initialize platform input state for a stream.
     *
     * @param config Configuration values to apply.
     * @param launch_session Launch session.
     * @return Allocated object or identifier, or an error value on failure.
     */
    std::shared_ptr<session_t> alloc(config_t &config, rtsp_stream::launch_session_t &launch_session);
    /**
     * @brief Start a streaming session for the supplied peer address.
     *
     * @param session Active streaming or pairing session for the request.
     * @param addr_string Addr string.
     * @return Start status.
     */
    int start(session_t &session, const std::string &addr_string);
    /**
     * @brief Stop a streaming session and prevent more packets from being queued.
     *
     * @param session Active streaming or pairing session for the request.
     */
    void stop(session_t &session);
    /**
     * @brief Wait for worker threads owned by the session to exit.
     *
     * @param session Active streaming or pairing session for the request.
     */
    void join(session_t &session);
    /**
     * @brief Platform handle returned from stream setup.
     *
     * @param session Active streaming or pairing session for the request.
     * @return Current lifecycle state for the stream session.
     */
    state_e state(session_t &session);
    /**
     * @brief Return the paired client certificate for a stream session.
     *
     * @param session Active streaming or pairing session for the request.
     * @return PEM certificate associated with the session's client.
     */
    const std::string &client_cert(session_t &session);
  }  // namespace session
}  // namespace stream
