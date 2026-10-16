#include "network/Ipv4Endpoint.hpp"
#include "network/TcpServerSocket.hpp"
#include "network/UdpSocket.hpp"
#include "rtsp/RtpReceiver.hpp"
#include "rtsp/RtspClientSession.hpp"
#include "rtsp/RtspParser.hpp"
#include "rtsp/RtspTransport.hpp"
#include "rtsp/SdpDescription.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <sys/socket.h>

#include <gtest/gtest.h>

namespace {

using voip::rtsp::ClientConfig;
using voip::rtsp::ClientSession;
using voip::rtsp::ClientState;
using voip::rtsp::Header;
using voip::rtsp::Method;
using voip::rtsp::Request;
using voip::rtsp::Response;
using voip::rtsp::SessionError;

struct ResponseStep
{
    int status_code{200};
    std::string reason{"OK"};
    std::vector<Header> headers;
    std::string body;
};

void send_all(const int socket_fd, const std::string_view payload)
{
    std::size_t offset = 0U;
    while (offset < payload.size()) {
        const auto sent = ::send(socket_fd, payload.data() + offset, payload.size() - offset, 0);
        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("test RTSP server failed to send response");
        }
        if (sent == 0) {
            throw std::runtime_error("test RTSP server send made no progress");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

std::string receive_headers(const int socket_fd)
{
    std::string request;
    char byte = '\0';
    while (!request.ends_with("\r\n\r\n")) {
        const auto received = ::recv(socket_fd, &byte, 1U, 0);
        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("test RTSP server failed to receive request");
        }
        if (received == 0) {
            throw std::runtime_error("RTSP client disconnected before completing its request");
        }
        request.push_back(byte);
    }
    return request;
}

void run_scripted_server(voip::network::TcpServerSocket &server,
                         const std::vector<ResponseStep> &steps,
                         std::vector<Request> &captured_requests, std::exception_ptr &server_error)
{
    try {
        auto peer = server.accept_one();
        for (const auto &step : steps) {
            auto request = voip::rtsp::parse_request(receive_headers(peer.get()));
            Response response{step.status_code, step.reason, request.cseq()};
            for (const auto &header : step.headers) {
                response.add_header(header.name, header.value);
            }
            response.set_body(step.body);
            captured_requests.push_back(std::move(request));
            send_all(peer.get(), response.serialize());
        }
    } catch (...) {
        server_error = std::current_exception();
    }
}

void rethrow_server_error(const std::exception_ptr &server_error)
{
    if (server_error) {
        std::rethrow_exception(server_error);
    }
}

std::string make_base_uri(const voip::network::Ipv4Endpoint &endpoint)
{
    return "rtsp://127.0.0.1:" + std::to_string(endpoint.port()) + "/demo";
}

struct RtpPacketFields
{
    std::uint16_t sequence{};
    std::uint32_t timestamp{};
};

std::vector<std::byte> make_rtp_packet(const RtpPacketFields &fields)
{
    const auto sequence = fields.sequence;
    const auto timestamp = fields.timestamp;
    std::vector<std::byte> packet(16U, std::byte{0});
    packet[0] = std::byte{0x80U};
    packet[1] = std::byte{0x00U};
    packet[2] = static_cast<std::byte>((sequence >> 8U) & 0xFFU);
    packet[3] = static_cast<std::byte>(sequence & 0xFFU);
    packet[4] = static_cast<std::byte>((timestamp >> 24U) & 0xFFU);
    packet[5] = static_cast<std::byte>((timestamp >> 16U) & 0xFFU);
    packet[6] = static_cast<std::byte>((timestamp >> 8U) & 0xFFU);
    packet[7] = static_cast<std::byte>(timestamp & 0xFFU);
    packet[8] = std::byte{0x12U};
    packet[9] = std::byte{0x34U};
    packet[10] = std::byte{0x56U};
    packet[11] = std::byte{0x78U};
    return packet;
}

std::uint16_t choose_rtcp_port(const std::uint16_t rtp_port)
{
    if (rtp_port == std::numeric_limits<std::uint16_t>::max()) {
        return static_cast<std::uint16_t>(rtp_port - 1U);
    }
    return static_cast<std::uint16_t>(rtp_port + 1U);
}

TEST(RtspEndToEndTest, ExecutesCompleteControlFlowAcrossLoopbackTcp)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();
    const auto base_uri = make_base_uri(server.local_endpoint());
    const auto sdp = std::string{"v=0\r\n"
                                 "s=phase10\r\n"
                                 "t=0 0\r\n"
                                 "m=audio 5004 RTP/AVP 0\r\n"
                                 "a=control:trackID=1\r\n"};

    const std::vector<ResponseStep> steps{
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN"}},
                     .body = ""},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Content-Type", "application/sdp"}},
                     .body = sdp},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "phase10-session;timeout=60"},
                                 Header{"Transport", "RTP/AVP;unicast;client_port=5000-5001;"
                                                     "server_port=7000-7001"}},
                     .body = ""},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "phase10-session"}},
                     .body = ""},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "phase10-session"}},
                     .body = ""},
    };

    std::vector<Request> captured_requests;
    std::exception_ptr server_error;
    std::jthread server_thread{run_scripted_server, std::ref(server), std::cref(steps),
                               std::ref(captured_requests), std::ref(server_error)};

    ClientSession session{ClientConfig{server.local_endpoint(), base_uri, 5000U, 5001U}};
    session.connect();
    static_cast<void>(session.options());
    const auto describe_response = session.describe();
    const auto description = voip::rtsp::parse_sdp(describe_response.body());
    const auto &track = voip::rtsp::select_first_rtp_track(description);
    ASSERT_TRUE(track.control.has_value());
    const auto control_uri = voip::rtsp::resolve_control_uri(
        {.base_uri = base_uri, .control = track.control.value_or("")});
    const auto setup_response = session.setup(control_uri);
    const auto transport_header = setup_response.header("Transport");
    ASSERT_TRUE(transport_header.has_value());
    const auto transport = voip::rtsp::parse_transport_header(transport_header.value_or(""));
    EXPECT_TRUE(transport.unicast);
    static_cast<void>(session.play());
    static_cast<void>(session.teardown());
    session.disconnect();

    server_thread.join();
    rethrow_server_error(server_error);

    ASSERT_EQ(captured_requests.size(), 5U);
    EXPECT_EQ(captured_requests[0].method(), Method::Options);
    EXPECT_EQ(captured_requests[1].method(), Method::Describe);
    EXPECT_EQ(captured_requests[2].method(), Method::Setup);
    EXPECT_EQ(captured_requests[3].method(), Method::Play);
    EXPECT_EQ(captured_requests[4].method(), Method::Teardown);
    EXPECT_EQ(captured_requests[2].uri(), control_uri);
    EXPECT_EQ(session.state(), ClientState::Disconnected);
}

TEST(RtspEndToEndTest, ReceivesRtpOnPortAdvertisedDuringSetup)
{
    voip::rtsp::RtpReceiver receiver{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    receiver.bind();
    const auto rtp_port = receiver.local_endpoint().port();
    const auto rtcp_port = choose_rtcp_port(rtp_port);

    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();
    const auto base_uri = make_base_uri(server.local_endpoint());
    const auto sdp = std::string{"v=0\r\n"
                                 "s=phase10\r\n"
                                 "t=0 0\r\n"
                                 "m=audio 5004 RTP/AVP 0\r\n"
                                 "a=control:trackID=1\r\n"};

    const std::vector<ResponseStep> steps{
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Content-Type", "application/sdp"}},
                     .body = sdp},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "media-session"},
                                 Header{"Transport", "RTP/AVP;unicast;server_port=7000-7001"}},
                     .body = ""},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "media-session"}},
                     .body = ""},
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Session", "media-session"}},
                     .body = ""},
    };

    std::vector<Request> captured_requests;
    std::exception_ptr server_error;
    std::jthread server_thread{run_scripted_server, std::ref(server), std::cref(steps),
                               std::ref(captured_requests), std::ref(server_error)};

    ClientSession session{ClientConfig{server.local_endpoint(), base_uri, rtp_port, rtcp_port}};
    session.connect();
    const auto describe_response = session.describe();
    const auto description = voip::rtsp::parse_sdp(describe_response.body());
    const auto &track = voip::rtsp::select_first_rtp_track(description);
    const auto control_uri = voip::rtsp::resolve_control_uri(
        {.base_uri = base_uri, .control = track.control.value_or("")});
    static_cast<void>(session.setup(control_uri));
    static_cast<void>(session.play());

    voip::network::UdpSocket sender{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    sender.bind();
    const auto packet = make_rtp_packet({
        .sequence = 77U,
        .timestamp = 3200U,
    });
    EXPECT_EQ(sender.send_to(packet, receiver.local_endpoint()), packet.size());
    const auto received = receiver.receive_one();
    EXPECT_EQ(received.header.sequence_number, 77U);
    EXPECT_EQ(received.header.timestamp, 3200U);
    EXPECT_EQ(received.statistics.packets_received, 1U);

    static_cast<void>(session.teardown());
    session.disconnect();
    server_thread.join();
    rethrow_server_error(server_error);

    ASSERT_EQ(captured_requests.size(), 4U);
    const auto setup_transport = captured_requests[1].header("Transport");
    ASSERT_TRUE(setup_transport.has_value());
    const auto advertised = voip::rtsp::parse_transport_header(setup_transport.value_or(""));
    ASSERT_TRUE(advertised.client_ports.has_value());
    EXPECT_EQ(advertised.client_ports.value_or(voip::rtsp::PortPair{}).rtp, rtp_port);
}

TEST(RtspEndToEndTest, SetupFailureDoesNotAdvanceClientState)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();
    const auto base_uri = make_base_uri(server.local_endpoint());
    const auto sdp = std::string{"v=0\r\n"
                                 "s=phase10\r\n"
                                 "t=0 0\r\n"
                                 "m=audio 5004 RTP/AVP 0\r\n"
                                 "a=control:trackID=1\r\n"};

    const std::vector<ResponseStep> steps{
        ResponseStep{.status_code = 200,
                     .reason = "OK",
                     .headers = {Header{"Content-Type", "application/sdp"}},
                     .body = sdp},
        ResponseStep{
            .status_code = 461, .reason = "Unsupported Transport", .headers = {}, .body = ""},
    };

    std::vector<Request> captured_requests;
    std::exception_ptr server_error;
    std::jthread server_thread{run_scripted_server, std::ref(server), std::cref(steps),
                               std::ref(captured_requests), std::ref(server_error)};

    ClientSession session{ClientConfig{server.local_endpoint(), base_uri, 5000U, 5001U}};
    session.connect();
    const auto describe_response = session.describe();
    const auto description = voip::rtsp::parse_sdp(describe_response.body());
    const auto &track = voip::rtsp::select_first_rtp_track(description);
    const auto control_uri = voip::rtsp::resolve_control_uri(
        {.base_uri = base_uri, .control = track.control.value_or("")});

    EXPECT_THROW(static_cast<void>(session.setup(control_uri)), SessionError);
    EXPECT_EQ(session.state(), ClientState::Described);
    EXPECT_FALSE(session.session_id().has_value());
    session.disconnect();

    server_thread.join();
    rethrow_server_error(server_error);
    ASSERT_EQ(captured_requests.size(), 2U);
    EXPECT_EQ(captured_requests.back().method(), Method::Setup);
}

} // namespace
