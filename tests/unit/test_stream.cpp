/**
 * @file tests/unit/test_stream.cpp
 * @brief Test src/stream.*
 */

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <boost/endian/conversion.hpp>

extern "C" {
  #include <moonlight-common-c/src/Video.h>
}

#include <src/stream.h>

namespace stream {
  std::vector<uint8_t> concat_and_insert(uint64_t insert_size, uint64_t slice_size, const std::string_view &data1, const std::string_view &data2);
}

#include "../tests_common.h"

TEST(ConcatAndInsertTests, ConcatNoInsertionTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(0, 2, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatLargeStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, sizeof(b1) + sizeof(b2) + 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatSmallStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e'};
  ASSERT_EQ(res, expected);
}

TEST(HostDisplayControlTest, SerializesDisplayList) {
  const auto payload = stream::make_host_display_list_payload({"Display A", "Display B"}, 1);

  ASSERT_EQ(payload.size(), 4 + 2 + 9 + 2 + 9);
  const auto *bytes = reinterpret_cast<const std::uint8_t *>(payload.data());
  EXPECT_EQ(bytes[0], 1);
  EXPECT_EQ(bytes[1], 0);
  EXPECT_EQ(bytes[2], 2);
  EXPECT_EQ(bytes[3], 0);
  EXPECT_EQ(bytes[4], 9);
  EXPECT_EQ(payload.substr(6, 9), "Display A");
}

TEST(HostDisplayControlTest, RejectsOversizedDisplayList) {
  EXPECT_TRUE(stream::make_host_display_list_payload({std::string(16 * 1024, 'x')}, 0).empty());
}

TEST(HostDisplayControlTest, ParsesValidSwitch) {
  const char payload[] = {1, 0};
  const auto index = stream::parse_host_display_switch(std::string_view(payload, sizeof(payload)), 2);
  ASSERT_TRUE(index);
  EXPECT_EQ(*index, 1);
}

TEST(HostDisplayControlTest, RejectsInvalidSwitch) {
  const char out_of_range[] = {2, 0};
  EXPECT_FALSE(stream::parse_host_display_switch(std::string_view(out_of_range, sizeof(out_of_range)), 2));
  EXPECT_FALSE(stream::parse_host_display_switch(std::string_view("\0", 1), 2));
}

namespace {
  std::string adaptive_quality_payload(std::uint32_t bitrate, std::uint16_t width, std::uint16_t height) {
    SS_ADAPTIVE_STREAM_CONFIG request {
      boost::endian::native_to_little(bitrate),
      boost::endian::native_to_little(width),
      boost::endian::native_to_little(height),
    };
    return {reinterpret_cast<const char *>(&request), sizeof(request)};
  }

  stream::config_t adaptive_quality_config() {
    stream::config_t config {};
    config.configuredBitrateKbps = 10000;
    config.monitor.width = 1920;
    config.monitor.height = 1080;
    config.monitor.bitrate = 7500;
    return config;
  }
}  // namespace

TEST(AdaptiveQualityRequestTest, ConvertsValidPayloadAndPreservesOtherVideoSettings) {
  auto config = adaptive_quality_config();
  config.monitor.framerate = 60;

  auto result = stream::make_adaptive_video_config(config, adaptive_quality_payload(6000, 1280, 720));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->width, 1280);
  EXPECT_EQ(result->height, 720);
  EXPECT_EQ(result->bitrate, 4500);
  EXPECT_EQ(result->framerate, 60);
}

TEST(AdaptiveQualityRequestTest, RejectsMalformedPayload) {
  auto payload = adaptive_quality_payload(6000, 1280, 720);
  payload.pop_back();

  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), payload));
}

TEST(AdaptiveQualityRequestTest, RejectsBitrateOutsideNegotiatedRange) {
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(499, 1280, 720)));
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(10001, 1280, 720)));
}

TEST(AdaptiveQualityRequestTest, RejectsInvalidResolution) {
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(6000, 319, 720)));
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(6000, 1280, 179)));
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(6000, 1922, 1080)));
  EXPECT_FALSE(stream::make_adaptive_video_config(adaptive_quality_config(), adaptive_quality_payload(6000, 1281, 720)));
}

TEST(AdaptiveQualityRequestTest, RejectsMissingConfiguredBitrate) {
  auto config = adaptive_quality_config();
  config.configuredBitrateKbps = 0;

  EXPECT_FALSE(stream::make_adaptive_video_config(config, adaptive_quality_payload(6000, 1280, 720)));
}

TEST(AdaptiveQualityRequestTest, KeepsEncoderBitrateAboveProtocolMinimum) {
  auto config = adaptive_quality_config();
  config.monitor.bitrate = 1000;

  auto result = stream::make_adaptive_video_config(config, adaptive_quality_payload(500, 640, 360));

  ASSERT_TRUE(result);
  EXPECT_EQ(result->bitrate, 500);
}
