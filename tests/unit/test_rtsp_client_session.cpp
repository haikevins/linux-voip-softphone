#include "network/Ipv4Endpoint.hpp"
#include "network/TcpServerSocket.hpp"
#include "rtsp/RtspClientSession.hpp"
#include "rtsp/RtspTcpConnection.hpp"

#include <cerrno>
#include <deque>
#include <exception>
#include <memory>
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
using voip::rtsp::Connection;
using voip::rtsp::Method;
using voip::rtsp::Request;
using voip::rtsp::Response;
using voip::rtsp::SessionError;

class FakeConnection final : public Connection
{
  public:
    void connect() override
    {
        connected_ = true;
    }

    void disconnect() noexcept override
    {
        connected_ = false;
    }

    [[nodiscard]] bool is_connected() const noexcept override
    {
        return connected_;
    }

    [[nodiscard]] Response transact(const Request &request) override
    {
        if (!connected_) {
            throw std::logic_error("fake RTSP connection is not connected");
        }
        if (responses_.empty()) {
            throw std::logic_error("fake RTSP response queue is empty");
        }

        requests_.push_back(request);
        auto response = std::move(responses_.front());
        responses_.pop_front();
        return response;
    }

    void queue_response(Response response)
    {
        responses_.push_back(std::move(response));
    }

    [[nodiscard]] const std::vector<Request> &requests() const noexcept
    {
        return requests_;
    }

  private:
    bool connected_{false};
    std::deque<Response> responses_;
    std::vector<Request> requests_;
};

ClientConfig make_config()
{
    return ClientConfig{voip::network::Ipv4Endpoint{"127.0.0.1", 8554U},
                        "rtsp://127.0.0.1:8554/demo", 5000U, 5001U};
}

Response ok_response(const int cseq)
{
    return Response{200, "OK", cseq};
}

struct SessionFixture
{
    SessionFixture()
    {
        auto connection = std::make_unique<FakeConnection>();
        fake = connection.get();
        session = std::make_unique<ClientSession>(make_config(), std::move(connection));
        session->connect();
    }

    FakeConnection *fake{nullptr};
    std::unique_ptr<ClientSession> session;
};

void queue_describe(FakeConnection &connection, const int cseq)
{
    auto response = ok_response(cseq);
    response.add_header("Content-Type", "application/sdp");
    response.set_body("v=0\r\n");
    connection.queue_response(std::move(response));
}

void complete_describe(SessionFixture &fixture)
{
    queue_describe(*fixture.fake, 1);
    static_cast<void>(fixture.session->describe());
}

void complete_setup(SessionFixture &fixture, const int cseq)
{
    auto response = ok_response(cseq);
    response.add_header("Session", "session-42;timeout=60");
    fixture.fake->queue_response(std::move(response));
    static_cast<void>(fixture.session->setup("rtsp://127.0.0.1:8554/demo/trackID=1"));
}

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

TEST(RtspClientSessionTest, SendsOptionsWithMonotonicCseq)
{
    SessionFixture fixture;
    fixture.fake->queue_response(ok_response(1));

    const auto response = fixture.session->options();

    EXPECT_EQ(response.status_code(), 200);
    ASSERT_EQ(fixture.fake->requests().size(), 1U);
    EXPECT_EQ(fixture.fake->requests().front().method(), Method::Options);
    EXPECT_EQ(fixture.fake->requests().front().cseq(), 1);
    EXPECT_EQ(fixture.session->state(), ClientState::Connected);
}

TEST(RtspClientSessionTest, DescribeRequestsSdpAndMovesToDescribedState)
{
    SessionFixture fixture;
    queue_describe(*fixture.fake, 1);

    const auto response = fixture.session->describe();

    ASSERT_EQ(fixture.fake->requests().size(), 1U);
    const auto accept = fixture.fake->requests().front().header("Accept");
    ASSERT_TRUE(accept.has_value());
    EXPECT_EQ(accept.value_or(""), "application/sdp");
    EXPECT_EQ(response.body(), "v=0\r\n");
    EXPECT_EQ(fixture.session->state(), ClientState::Described);
}

TEST(RtspClientSessionTest, SetupCapturesSessionIdentifierAndTransportPorts)
{
    SessionFixture fixture;
    complete_describe(fixture);
    complete_setup(fixture, 2);

    ASSERT_EQ(fixture.fake->requests().size(), 2U);
    const auto &request = fixture.fake->requests().back();
    EXPECT_EQ(request.method(), Method::Setup);
    EXPECT_EQ(request.cseq(), 2);
    const auto transport = request.header("Transport");
    ASSERT_TRUE(transport.has_value());
    EXPECT_EQ(transport.value_or(""), "RTP/AVP;unicast;client_port=5000-5001");
    EXPECT_EQ(fixture.session->session_id().value_or(""), "session-42");
    EXPECT_EQ(fixture.session->state(), ClientState::Ready);
}

TEST(RtspClientSessionTest, PlayAndTeardownUseNegotiatedSession)
{
    SessionFixture fixture;
    complete_describe(fixture);
    complete_setup(fixture, 2);
    fixture.fake->queue_response(ok_response(3));
    fixture.fake->queue_response(ok_response(4));

    static_cast<void>(fixture.session->play());
    EXPECT_EQ(fixture.session->state(), ClientState::Playing);
    static_cast<void>(fixture.session->teardown());

    ASSERT_EQ(fixture.fake->requests().size(), 4U);
    const auto play_session = fixture.fake->requests()[2].header("Session");
    const auto teardown_session = fixture.fake->requests()[3].header("Session");
    EXPECT_EQ(play_session.value_or(""), "session-42");
    EXPECT_EQ(teardown_session.value_or(""), "session-42");
    EXPECT_FALSE(fixture.session->session_id().has_value());
    EXPECT_EQ(fixture.session->state(), ClientState::Connected);
}

TEST(RtspClientSessionTest, RejectsOutOfOrderSessionMethods)
{
    SessionFixture fixture;

    EXPECT_THROW(static_cast<void>(fixture.session->setup("rtsp://127.0.0.1/trackID=1")),
                 std::logic_error);
    EXPECT_THROW(static_cast<void>(fixture.session->play()), std::logic_error);
    EXPECT_THROW(static_cast<void>(fixture.session->teardown()), std::logic_error);
}

TEST(RtspClientSessionTest, RejectsMismatchedCseq)
{
    SessionFixture fixture;
    fixture.fake->queue_response(ok_response(9));

    EXPECT_THROW(static_cast<void>(fixture.session->options()), SessionError);
}

TEST(RtspClientSessionTest, RejectsNonSuccessfulRtspStatus)
{
    SessionFixture fixture;
    fixture.fake->queue_response(Response{404, "Not Found", 1});

    EXPECT_THROW(static_cast<void>(fixture.session->options()), SessionError);
}

TEST(RtspTcpConnectionTest, RoundTripsOneRtspResponseAcrossLoopback)
{
    voip::network::TcpServerSocket server{voip::network::Ipv4Endpoint{"127.0.0.1", 0U}};
    server.start();

    voip::rtsp::TcpConnection connection{server.local_endpoint()};
    connection.connect();

    std::string captured_request;
    std::exception_ptr server_error;
    std::jthread server_thread([&server, &captured_request, &server_error] {
        try {
            auto peer = server.accept_one();
            captured_request = receive_headers(peer.get());
            send_all(peer.get(), "RTSP/1.0 200 OK\r\nCSeq: 1\r\nContent-Length: 0\r\n\r\n");
        } catch (...) {
            server_error = std::current_exception();
        }
    });

    Request request{Method::Options, "rtsp://127.0.0.1/demo", 1};
    const auto response = connection.transact(request);
    server_thread.join();
    if (server_error) {
        std::rethrow_exception(server_error);
    }

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.cseq(), 1);
    EXPECT_TRUE(captured_request.starts_with("OPTIONS rtsp://127.0.0.1/demo RTSP/1.0\r\n"));
}

} // namespace
