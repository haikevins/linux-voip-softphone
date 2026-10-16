#include "rtsp/RtpPacket.hpp"
#include "rtsp/RtpReceiver.hpp"
#include "rtsp/RtspTransport.hpp"
#include "rtsp/SdpDescription.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace {

using voip::rtsp::ControlUriInput;
using voip::rtsp::parse_rtp_packet_header;
using voip::rtsp::parse_sdp;
using voip::rtsp::parse_transport_header;
using voip::rtsp::resolve_control_uri;
using voip::rtsp::RtpPacketError;
using voip::rtsp::RtpStreamTracker;
using voip::rtsp::SdpError;
using voip::rtsp::select_first_rtp_track;
using voip::rtsp::TransportError;

struct RtpPacketSeed
{
    std::uint16_t sequence{0U};
    std::uint32_t timestamp{0U};
};

std::array<std::byte, 16U> make_rtp_packet(const RtpPacketSeed seed)
{
    std::array<std::byte, 16U> packet{};
    packet[0] = std::byte{0x80U};
    packet[1] = std::byte{0x80U};
    packet[2] = static_cast<std::byte>((seed.sequence >> 8U) & 0xFFU);
    packet[3] = static_cast<std::byte>(seed.sequence & 0xFFU);
    packet[4] = static_cast<std::byte>((seed.timestamp >> 24U) & 0xFFU);
    packet[5] = static_cast<std::byte>((seed.timestamp >> 16U) & 0xFFU);
    packet[6] = static_cast<std::byte>((seed.timestamp >> 8U) & 0xFFU);
    packet[7] = static_cast<std::byte>(seed.timestamp & 0xFFU);
    packet[8] = std::byte{0x12U};
    packet[9] = std::byte{0x34U};
    packet[10] = std::byte{0x56U};
    packet[11] = std::byte{0x78U};
    return packet;
}

TEST(Phase10SdpTest, ParsesRtpMediaAndControlAttribute)
{
    const auto description = parse_sdp("v=0\r\n"
                                       "o=- 1 1 IN IP4 127.0.0.1\r\n"
                                       "s=demo\r\n"
                                       "t=0 0\r\n"
                                       "a=control:*\r\n"
                                       "m=audio 5004 RTP/AVP 0 96\r\n"
                                       "a=control:trackID=1\r\n");

    ASSERT_EQ(description.media_tracks.size(), 1U);
    EXPECT_EQ(description.session_control.value_or(""), "*");
    const auto &track = description.media_tracks.front();
    EXPECT_EQ(track.media, "audio");
    EXPECT_EQ(track.port, 5004U);
    EXPECT_EQ(track.protocol, "RTP/AVP");
    EXPECT_EQ(track.payload_types, (std::vector<int>{0, 96}));
    EXPECT_EQ(track.control.value_or(""), "trackID=1");
}

TEST(Phase10SdpTest, SelectsFirstRtpTrack)
{
    const auto description = parse_sdp("v=0\n"
                                       "m=application 9 TCP/RTSP 98\n"
                                       "m=audio 5004 RTP/AVP 0\n");

    EXPECT_EQ(select_first_rtp_track(description).media, "audio");
}

TEST(Phase10SdpTest, ResolvesRelativeAndAbsoluteControlUris)
{
    EXPECT_EQ(resolve_control_uri(ControlUriInput{.base_uri = "rtsp://127.0.0.1:8554/demo",
                                                  .control = "trackID=1"}),
              "rtsp://127.0.0.1:8554/demo/trackID=1");
    EXPECT_EQ(resolve_control_uri(ControlUriInput{.base_uri = "rtsp://127.0.0.1:8554/demo",
                                                  .control = "rtsp://media.test/track"}),
              "rtsp://media.test/track");
}

TEST(Phase10SdpTest, RejectsMalformedMediaPort)
{
    EXPECT_THROW(parse_sdp("v=0\nm=audio nope RTP/AVP 0\n"), SdpError);
}

TEST(Phase10TransportTest, ParsesClientAndServerPortPairs)
{
    const auto transport =
        parse_transport_header("RTP/AVP;unicast;client_port=5000-5001;server_port=7000-7001");

    EXPECT_TRUE(transport.unicast);
    ASSERT_TRUE(transport.client_ports.has_value());
    ASSERT_TRUE(transport.server_ports.has_value());
    const auto client_ports = transport.client_ports.value_or(voip::rtsp::PortPair{});
    const auto server_ports = transport.server_ports.value_or(voip::rtsp::PortPair{});
    EXPECT_EQ(client_ports.rtp, 5000U);
    EXPECT_EQ(client_ports.rtcp, 5001U);
    EXPECT_EQ(server_ports.rtp, 7000U);
    EXPECT_EQ(server_ports.rtcp, 7001U);
}

TEST(Phase10TransportTest, RejectsNonUdpTransport)
{
    EXPECT_THROW(parse_transport_header("RTP/AVP/TCP;unicast;interleaved=0-1"), TransportError);
}

TEST(Phase10RtpPacketTest, ParsesFixedHeaderFields)
{
    const auto packet =
        make_rtp_packet(RtpPacketSeed{.sequence = 0x1234U, .timestamp = 0x01020304U});
    const auto header = parse_rtp_packet_header(packet);

    EXPECT_TRUE(header.marker);
    EXPECT_EQ(header.payload_type, 0U);
    EXPECT_EQ(header.sequence_number, 0x1234U);
    EXPECT_EQ(header.timestamp, 0x01020304U);
    EXPECT_EQ(header.ssrc, 0x12345678U);
    EXPECT_EQ(header.header_size, 12U);
}

TEST(Phase10RtpPacketTest, AccountsForCsrcAndExtensionHeaders)
{
    std::array<std::byte, 24U> packet{};
    packet[0] = std::byte{0x91U};
    packet[1] = std::byte{0x60U};
    packet[16] = std::byte{0xBEU};
    packet[17] = std::byte{0xDEU};
    packet[18] = std::byte{0x00U};
    packet[19] = std::byte{0x01U};

    EXPECT_EQ(parse_rtp_packet_header(packet).header_size, 24U);
}

TEST(Phase10RtpPacketTest, RejectsInvalidVersion)
{
    auto packet = make_rtp_packet(RtpPacketSeed{.sequence = 1U, .timestamp = 1U});
    packet[0] = std::byte{0x40U};
    EXPECT_THROW(parse_rtp_packet_header(packet), RtpPacketError);
}

TEST(Phase10RtpTrackerTest, CountsSequenceGapsAsObservedLoss)
{
    RtpStreamTracker tracker;
    tracker.observe(parse_rtp_packet_header(
        make_rtp_packet(RtpPacketSeed{.sequence = 100U, .timestamp = 1000U})));
    tracker.observe(parse_rtp_packet_header(
        make_rtp_packet(RtpPacketSeed{.sequence = 101U, .timestamp = 1160U})));
    tracker.observe(parse_rtp_packet_header(
        make_rtp_packet(RtpPacketSeed{.sequence = 104U, .timestamp = 1640U})));

    const auto statistics = tracker.statistics();
    EXPECT_EQ(statistics.packets_received, 3U);
    EXPECT_EQ(statistics.packets_lost, 2U);
    EXPECT_DOUBLE_EQ(statistics.packet_loss_percent, 40.0);
}

TEST(Phase10RtpTrackerTest, HandlesSequenceNumberWrap)
{
    RtpStreamTracker tracker;
    tracker.observe(parse_rtp_packet_header(
        make_rtp_packet(RtpPacketSeed{.sequence = 65535U, .timestamp = 1000U})));
    tracker.observe(parse_rtp_packet_header(
        make_rtp_packet(RtpPacketSeed{.sequence = 0U, .timestamp = 1160U})));

    const auto statistics = tracker.statistics();
    EXPECT_EQ(statistics.packets_received, 2U);
    EXPECT_EQ(statistics.packets_lost, 0U);
}

TEST(Phase10RtpReceiverTest, BindsAndReceivesUdpRtpPacket)
{
    voip::rtsp::RtpReceiver receiver{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    receiver.bind();

    voip::network::UdpSocket sender{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    sender.bind();
    const auto packet = make_rtp_packet(RtpPacketSeed{.sequence = 42U, .timestamp = 3200U});
    EXPECT_EQ(sender.send_to(packet, receiver.local_endpoint()), packet.size());

    const auto result = receiver.receive_one();
    EXPECT_EQ(result.header.sequence_number, 42U);
    EXPECT_EQ(result.peer_endpoint, sender.local_endpoint());
    EXPECT_EQ(result.statistics.packets_received, 1U);
    EXPECT_EQ(result.statistics.packets_lost, 0U);
}

TEST(Phase10RtpReceiverTest, ReportsSequenceGapAcrossUdpDatagrams)
{
    voip::rtsp::RtpReceiver receiver{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    receiver.bind();

    voip::network::UdpSocket sender{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    sender.bind();
    const auto first = make_rtp_packet(RtpPacketSeed{.sequence = 10U, .timestamp = 1600U});
    const auto second = make_rtp_packet(RtpPacketSeed{.sequence = 12U, .timestamp = 1920U});
    EXPECT_EQ(sender.send_to(first, receiver.local_endpoint()), first.size());
    EXPECT_EQ(receiver.receive_one().header.sequence_number, 10U);
    EXPECT_EQ(sender.send_to(second, receiver.local_endpoint()), second.size());
    EXPECT_EQ(receiver.receive_one().header.sequence_number, 12U);

    const auto statistics = receiver.statistics();
    EXPECT_EQ(statistics.packets_received, 2U);
    EXPECT_EQ(statistics.packets_lost, 1U);
}

} // namespace
