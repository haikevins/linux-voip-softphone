#include "rtsp/RtspMessage.hpp"
#include "rtsp/RtspParser.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace {

using voip::rtsp::Method;
using voip::rtsp::ParseError;
using voip::rtsp::Request;
using voip::rtsp::Response;

TEST(RtspProtocolTest, MapsSupportedMethods)
{
    EXPECT_EQ(voip::rtsp::parse_method("OPTIONS"), Method::Options);
    EXPECT_EQ(voip::rtsp::parse_method("DESCRIBE"), Method::Describe);
    EXPECT_EQ(voip::rtsp::parse_method("SETUP"), Method::Setup);
    EXPECT_EQ(voip::rtsp::parse_method("PLAY"), Method::Play);
    EXPECT_EQ(voip::rtsp::parse_method("TEARDOWN"), Method::Teardown);
    EXPECT_FALSE(voip::rtsp::parse_method("PAUSE").has_value());
}

TEST(RtspProtocolTest, SerializesRequestWithCanonicalCseqAndContentLength)
{
    Request request{Method::Describe, "rtsp://127.0.0.1:8554/demo", 7};
    request.add_header("Accept", "application/sdp");
    request.set_body("abc");

    EXPECT_EQ(request.serialize(), "DESCRIBE rtsp://127.0.0.1:8554/demo RTSP/1.0\r\n"
                                   "CSeq: 7\r\n"
                                   "Accept: application/sdp\r\n"
                                   "Content-Length: 3\r\n"
                                   "\r\n"
                                   "abc");
}

TEST(RtspProtocolTest, ParsesRequestAndProvidesCaseInsensitiveHeaderLookup)
{
    const auto request =
        voip::rtsp::parse_request("SETUP rtsp://example.test/media/trackID=1 RTSP/1.0\r\n"
                                  "CSeq: 12\r\n"
                                  "Transport: RTP/AVP;unicast;client_port=5000-5001\r\n"
                                  "\r\n");

    EXPECT_EQ(request.method(), Method::Setup);
    EXPECT_EQ(request.uri(), "rtsp://example.test/media/trackID=1");
    EXPECT_EQ(request.cseq(), 12);
    ASSERT_TRUE(request.header("transport").has_value());
    const auto transport = request.header("TRANSPORT");
    ASSERT_TRUE(transport.has_value());
    EXPECT_EQ(transport.value_or(""), "RTP/AVP;unicast;client_port=5000-5001");
}

TEST(RtspProtocolTest, ParsesResponseBodyUsingContentLength)
{
    constexpr auto sdp = "v=0\r\nm=audio 0 RTP/AVP 0\r\n";
    const std::string message = "RTSP/1.0 200 OK\r\n"
                                "CSeq: 3\r\n"
                                "Content-Type: application/sdp\r\n"
                                "Content-Length: 26\r\n"
                                "\r\n" +
                                std::string{sdp};

    const auto response = voip::rtsp::parse_response(message);

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.reason(), "OK");
    EXPECT_EQ(response.cseq(), 3);
    EXPECT_EQ(response.body(), sdp);
    ASSERT_TRUE(response.header("content-type").has_value());
    const auto content_type = response.header("Content-Type");
    ASSERT_TRUE(content_type.has_value());
    EXPECT_EQ(content_type.value_or(""), "application/sdp");
}

TEST(RtspProtocolTest, RejectsMissingCseq)
{
    EXPECT_THROW(voip::rtsp::parse_response("RTSP/1.0 200 OK\r\nServer: test\r\n\r\n"), ParseError);
}

TEST(RtspProtocolTest, RejectsContentLengthMismatch)
{
    EXPECT_THROW(voip::rtsp::parse_response("RTSP/1.0 200 OK\r\n"
                                            "CSeq: 1\r\n"
                                            "Content-Length: 4\r\n"
                                            "\r\n"
                                            "abc"),
                 ParseError);
}

TEST(RtspProtocolTest, RejectsHeaderInjectionWhenConstructingMessages)
{
    Request request{Method::Options, "rtsp://example.test/media", 1};
    EXPECT_THROW(request.add_header("X-Test", "ok\r\nInjected: yes"), std::invalid_argument);

    Response response{200, "OK", 1};
    EXPECT_THROW(response.add_header("Bad:Name", "value"), std::invalid_argument);
}

} // namespace
