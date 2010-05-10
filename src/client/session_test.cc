// Copyright 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <string>
#include <map>
#include "base/base.h"
#include "base/util.h"
#include "base/version.h"
#include "client/session.h"
#include "ipc/ipc_mock.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace client {

namespace {
const string UpdateVersion(int diff) {
  vector<string> tokens;
  Util::SplitStringUsing(Version::GetMozcVersion(), ".", &tokens);
  EXPECT_EQ(tokens.size(), 4);
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", Util::SimpleAtoi(tokens[3]) + diff);
  tokens[3] = buf;
  string output;
  Util::JoinStrings(tokens, ".", &output);
  return output;
}
}

class TestStartServerHandler : public StartServerHandlerInterface {
 public:
  TestStartServerHandler(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        force_terminate_server_result_(false),
        force_terminate_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  virtual bool StartServer(SessionInterface *session) {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    if (!product_version_after_start_server_.empty()) {
      factory_->SetServerProductVersion(product_version_after_start_server_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  virtual bool ForceTerminateServer(const string &name) {
    force_terminate_server_called_ = true;
    return force_terminate_server_result_;
  }

  virtual bool WaitServer(uint32 pid) {
    return true;
  }

  virtual void OnFatal(StartServerHandlerInterface::ServerErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
    error_map_[static_cast<int>(type)]++;
  }

  int error_count(StartServerHandlerInterface::ServerErrorType type) {
    return error_map_[static_cast<int>(type)];
  }

  bool start_server_called() const {
    return start_server_called_;
  }

  void set_start_server_called(bool start_server_called) {
    start_server_called_ = start_server_called;
  }

  bool force_terminate_server_called() const {
    return force_terminate_server_called_;
  }

  void set_force_terminate_server_called(bool force_terminate_server_called) {
    force_terminate_server_called_ = force_terminate_server_called;
  }

  virtual const string &server_program() const {
    static const string path;
    return path;
  }

  void set_restricted(bool restricted) {}

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  void set_force_terminate_server_result(const bool result) {
    force_terminate_server_result_ = result;
  }

  void set_server_protocol_version(uint32 server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }

  uint32 set_server_protocol_version() const {
    return server_protocol_version_;
  }

  void set_mock_after_start_server(const commands::Output &mock_output) {
    mock_output.SerializeToString(&response_);
  }

  void set_product_version_after_start_server(const string &version) {
    product_version_after_start_server_ = version;
  }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  bool force_terminate_server_result_;
  bool force_terminate_server_called_;
  uint32 server_protocol_version_;
  string response_;
  string product_version_after_start_server_;
  map<int, int> error_map_;
};

class SessionTest : public testing::Test {
 protected:
  SessionTest() : version_diff_(0) {}

  virtual void SetUp() {
    client_factory_.reset(new IPCClientFactoryMock);
    session_.reset(new Session);
    session_->SetIPCClientFactory(client_factory_.get());

    start_server_handler_ = new TestStartServerHandler(client_factory_.get());
    session_->SetStartServerHandler(start_server_handler_);
  }

  virtual void TearDown() {
    session_.reset();
    client_factory_.reset();
  }

  void SetMockOutput(const commands::Output &mock_output) {
    string response;
    mock_output.SerializeToString(&response);
    client_factory_->SetMockResponse(response);
  }

  void GetGeneratedInput(commands::Input *input) {
    input->ParseFromString(client_factory_->GetGeneratedRequest());
  }

  void SetupProductVersion(int version_diff) {
    version_diff_ = version_diff;
  }

  bool SetupConnection(const int id) {
    client_factory_->SetConnection(true);
    client_factory_->SetResult(true);
    if (version_diff_ == 0) {
      client_factory_->SetServerProductVersion(Version::GetMozcVersion());
    } else {
      client_factory_->SetServerProductVersion(UpdateVersion(version_diff_));
    }
    start_server_handler_->set_start_server_result(true);

    // TODO(komatsu): Due to the limitation of the testing mock,
    // EnsureConnection should be explicitly called before calling
    // SendKey.  Fix the testing mock.
    commands::Output mock_output;
    mock_output.set_id(id);
    SetMockOutput(mock_output);
    return session_->EnsureConnection();
  }

  scoped_ptr<IPCClientFactoryMock> client_factory_;
  scoped_ptr<Session> session_;
  TestStartServerHandler *start_server_handler_;
  int version_diff_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionTest);
};

TEST_F(SessionTest, ConnectionError) {
  client_factory_->SetConnection(false);
  start_server_handler_->set_start_server_result(false);
  EXPECT_FALSE(session_->EnsureConnection());

  commands::KeyEvent key;
  commands::Output output;
  EXPECT_FALSE(session_->SendKey(key, &output));

  key.Clear();
  output.Clear();
  EXPECT_FALSE(session_->TestSendKey(key, &output));

  commands::SessionCommand command;
  output.Clear();
  EXPECT_FALSE(session_->SendCommand(command, &output));
}

TEST_F(SessionTest, SendKey) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(session_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_KEY, input.type());
}

TEST_F(SessionTest, TestSendKey) {
  const int mock_id = 512;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(session_->TestSendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::TEST_SEND_KEY, input.type());
}

TEST_F(SessionTest, SendCommand) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(session_->SendCommand(session_command, &output));

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_COMMAND, input.type());
}

TEST_F(SessionTest, SetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  config::Config config;
  EXPECT_TRUE(session_->SetConfig(config));
}

TEST_F(SessionTest, GetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.mutable_config()->set_verbose_level(2);
  mock_output.mutable_config()->set_incognito_mode(true);
  SetMockOutput(mock_output);

  config::Config config;
  EXPECT_TRUE(session_->GetConfig(&config));

  EXPECT_EQ(2, config.verbose_level());
  EXPECT_EQ(true, config.incognito_mode());
}

TEST_F(SessionTest, EnableCascadingWindow) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  session_->NoOperation();
  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_FALSE(input.has_config());

  session_->EnableCascadingWindow(false);
  session_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_FALSE(input.config().use_cascading_window());

  session_->EnableCascadingWindow(true);
  session_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_TRUE(input.config().use_cascading_window());

  session_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  EXPECT_TRUE(input.config().has_use_cascading_window());
}

TEST_F(SessionTest, VersionMismatch) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  // Suddenly, connects to a different server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION + 1);
  commands::Output output;
  EXPECT_FALSE(session_->SendKey(key_event, &output));
  EXPECT_FALSE(session_->EnsureConnection());
  EXPECT_EQ(1, start_server_handler_->error_count
            (StartServerHandlerInterface::SERVER_VERSION_MISMATCH));
}

TEST_F(SessionTest, ProtocolUpdate) {
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  start_server_handler_->set_force_terminate_server_called(false);
  start_server_handler_->set_force_terminate_server_result(true);
  start_server_handler_->set_start_server_called(false);

  // Now connecting to an old server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // after start server, protocol version becomes the same
  start_server_handler_->set_server_protocol_version(IPC_PROTOCOL_VERSION);

  EXPECT_TRUE(session_->EnsureSession());
  EXPECT_TRUE(start_server_handler_->start_server_called());
  EXPECT_TRUE(start_server_handler_->force_terminate_server_called());
}

TEST_F(SessionTest, ProtocolUpdateFailSameBinary) {
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  start_server_handler_->set_force_terminate_server_called(false);
  start_server_handler_->set_force_terminate_server_result(true);
  start_server_handler_->set_start_server_called(false);

  EXPECT_FALSE(start_server_handler_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  start_server_handler_->set_server_protocol_version(IPC_PROTOCOL_VERSION - 1);
  start_server_handler_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(session_->EnsureSession());
  EXPECT_TRUE(start_server_handler_->start_server_called());
  EXPECT_TRUE(start_server_handler_->force_terminate_server_called());
  EXPECT_FALSE(session_->EnsureConnection());
  EXPECT_EQ(1, start_server_handler_->error_count
            (StartServerHandlerInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(SessionTest, ProtocolUpdateFailOnTerminate) {
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  start_server_handler_->set_force_terminate_server_called(false);
  start_server_handler_->set_force_terminate_server_result(false);
  start_server_handler_->set_start_server_called(false);

  EXPECT_FALSE(start_server_handler_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  start_server_handler_->set_server_protocol_version(IPC_PROTOCOL_VERSION);
  start_server_handler_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(session_->EnsureSession());
  EXPECT_FALSE(start_server_handler_->start_server_called());
  EXPECT_TRUE(start_server_handler_->force_terminate_server_called());
  EXPECT_FALSE(session_->EnsureConnection());
  EXPECT_EQ(1, start_server_handler_->error_count
            (StartServerHandlerInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(SessionTest, ServerUpdate) {
  SetupProductVersion(-1);  // old version
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  start_server_handler_->set_start_server_called(false);
  EXPECT_FALSE(start_server_handler_->start_server_called());

  // version is updated after restart the server
  start_server_handler_->set_product_version_after_start_server(
      Version::GetMozcVersion());
  EXPECT_TRUE(session_->EnsureSession());
  EXPECT_TRUE(start_server_handler_->start_server_called());
}

TEST_F(SessionTest, ServerUpdateToNewer) {
  SetupProductVersion(1);  // new version
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());
  start_server_handler_->set_start_server_called(false);
  EXPECT_TRUE(session_->EnsureSession());
  EXPECT_FALSE(start_server_handler_->start_server_called());
}

TEST_F(SessionTest, ServerUpdateFail) {
  SetupProductVersion(-1);  // old
  start_server_handler_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(session_->EnsureConnection());

  start_server_handler_->set_start_server_called(false);
  EXPECT_FALSE(start_server_handler_->start_server_called());

  // version is not updated after restart the server
  start_server_handler_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(session_->EnsureSession());
  EXPECT_TRUE(start_server_handler_->start_server_called());
  EXPECT_FALSE(session_->EnsureConnection());
  EXPECT_EQ(1, start_server_handler_->error_count
            (StartServerHandlerInterface::SERVER_BROKEN_MESSAGE));
}
}  // namespace client
}  // namespace mozc
