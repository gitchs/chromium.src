// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_reliable_server_stream.h"

#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_spdy_server_stream.h"
#include "net/tools/quic/spdy_utils.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::tools::test::MockConnection;
using net::test::MockSession;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::InvokeArgument;
using testing::InSequence;
using testing::Return;
using testing::StrEq;
using testing::StrictMock;
using testing::WithArgs;

namespace net {
namespace tools {
namespace test {

class QuicReliableServerStreamPeer {
 public:
  static BalsaHeaders* GetMutableHeaders(
      QuicReliableServerStream* stream) {
    return &(stream->headers_);
  }
};

namespace {

class QuicReliableServerStreamTest : public ::testing::Test {
 public:
  QuicReliableServerStreamTest()
      : session_(new MockConnection(1, IPEndPoint(), 0, &eps_, true), true),
        body_("hello world") {
    BalsaHeaders request_headers;
    request_headers.SetRequestFirstlineFromStringPieces(
        "POST", "https://www.google.com/", "HTTP/1.1");
    request_headers.ReplaceOrAppendHeader("content-length", "11");

    headers_string_ = SpdyUtils::SerializeRequestHeaders(request_headers);
    stream_.reset(new QuicSpdyServerStream(3, &session_));
  }

  QuicConsumedData ValidateHeaders(const struct iovec* iov) {
    StringPiece headers =
        StringPiece(static_cast<const char*>(iov[0].iov_base), iov[0].iov_len);
    headers_string_ = SpdyUtils::SerializeResponseHeaders(
        response_headers_);
    QuicSpdyDecompressor decompressor;
    TestDecompressorVisitor visitor;

    // First the header id, then the compressed data.
    EXPECT_EQ(1, headers[0]);
    EXPECT_EQ(0, headers[1]);
    EXPECT_EQ(0, headers[2]);
    EXPECT_EQ(0, headers[3]);
    EXPECT_EQ(static_cast<size_t>(headers.length() - 4),
              decompressor.DecompressData(headers.substr(4), &visitor));

    EXPECT_EQ(headers_string_, visitor.data());

    return QuicConsumedData(headers.size(), false);
  }

  static void SetUpTestCase() {
    QuicInMemoryCachePeer::ResetForTests();
  }

  virtual void SetUp() {
    QuicInMemoryCache* cache = QuicInMemoryCache::GetInstance();

    BalsaHeaders request_headers, response_headers;
    StringPiece body("Yum");
    request_headers.SetRequestFirstlineFromStringPieces(
        "GET",
        "https://www.google.com/foo",
        "HTTP/1.1");
    response_headers.SetRequestFirstlineFromStringPieces("HTTP/1.1",
                                                         "200",
                                                         "OK");
    response_headers.AppendHeader("content-length",
                                  base::IntToString(body.length()));

    // Check if response already exists and matches.
    const QuicInMemoryCache::Response* cached_response =
        cache->GetResponse(request_headers);
    if (cached_response != NULL) {
      string cached_response_headers_str, response_headers_str;
      cached_response->headers().DumpToString(&cached_response_headers_str);
      response_headers.DumpToString(&response_headers_str);
      CHECK_EQ(cached_response_headers_str, response_headers_str);
      CHECK_EQ(cached_response->body(), body);
      return;
    }

    cache->AddResponse(request_headers, response_headers, body);
  }

  BalsaHeaders response_headers_;
  EpollServer eps_;
  StrictMock<MockSession> session_;
  scoped_ptr<QuicReliableServerStream> stream_;
  string headers_string_;
  string body_;
};

QuicConsumedData ConsumeAllData(QuicStreamId id,
                                const struct iovec* iov,
                                int iov_count,
                                QuicStreamOffset offset,
                                bool fin) {
  ssize_t consumed_length = 0;
  for (int i = 0; i < iov_count; ++i) {
    consumed_length += iov[i].iov_len;
  }
  return QuicConsumedData(consumed_length, fin);
}

TEST_F(QuicReliableServerStreamTest, TestFraming) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  EXPECT_EQ(body_.size(), stream_->ProcessData(body_.c_str(), body_.size()));
  EXPECT_EQ(11u, stream_->headers().content_length());
  EXPECT_EQ("https://www.google.com/", stream_->headers().request_uri());
  EXPECT_EQ("POST", stream_->headers().request_method());
  EXPECT_EQ(body_, stream_->body());
}

TEST_F(QuicReliableServerStreamTest, TestFramingOnePacket) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  string message = headers_string_ + body_;

  EXPECT_EQ(message.size(), stream_->ProcessData(
      message.c_str(), message.size()));
  EXPECT_EQ(11u, stream_->headers().content_length());
  EXPECT_EQ("https://www.google.com/",
            stream_->headers().request_uri());
  EXPECT_EQ("POST", stream_->headers().request_method());
  EXPECT_EQ(body_, stream_->body());
}

TEST_F(QuicReliableServerStreamTest, TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  // We'll automatically write out an error (headers + body)
  EXPECT_CALL(session_, WritevData(_, _, _, _, _)).Times(2).
      WillRepeatedly(Invoke(ConsumeAllData));

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  // Content length is still 11.  This will register as an error and we won't
  // accept the bytes.
  stream_->ProcessData(large_body.c_str(), large_body.size());
  stream_->TerminateFromPeer(true);
  EXPECT_EQ(11u, stream_->headers().content_length());
  EXPECT_EQ("https://www.google.com/", stream_->headers().request_uri());
  EXPECT_EQ("POST", stream_->headers().request_method());
}

TEST_F(QuicReliableServerStreamTest, TestSendResponse) {
  BalsaHeaders* request_headers =
      QuicReliableServerStreamPeer::GetMutableHeaders(stream_.get());
  request_headers->SetRequestFirstlineFromStringPieces(
      "GET",
      "https://www.google.com/foo",
      "HTTP/1.1");

  response_headers_.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "200", "OK");
  response_headers_.ReplaceOrAppendHeader("content-length", "3");

  InSequence s;
  EXPECT_CALL(session_, WritevData(_, _, 1, _, _)).Times(1)
      .WillOnce(WithArgs<1>(Invoke(
          this, &QuicReliableServerStreamTest::ValidateHeaders)));

  EXPECT_CALL(session_, WritevData(_, _, 1, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  stream_->SendResponse();
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicReliableServerStreamTest, TestSendErrorResponse) {
  response_headers_.SetResponseFirstlineFromStringPieces(
      "HTTP/1.1", "500", "Server Error");
  response_headers_.ReplaceOrAppendHeader("content-length", "3");

  InSequence s;
  EXPECT_CALL(session_, WritevData(_, _, 1, _, _)).Times(1)
      .WillOnce(WithArgs<1>(Invoke(
          this, &QuicReliableServerStreamTest::ValidateHeaders)));

  EXPECT_CALL(session_, WritevData(_, _, 1, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  stream_->SendErrorResponse();
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
