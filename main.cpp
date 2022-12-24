#include <coroutine>
#include <cstdio>

#include "HelloWorld.grpc.pb.h"
#include "HelloWorld.pb.h"
#include "cppcoro/generator.hpp"
#include "cppcoro/single_consumer_event.hpp"
#include "cppcoro/sync_wait.hpp"
#include "cppcoro/task.hpp"
#include "cppcoro/when_all.hpp"
#include "cppcoro/when_all_ready.hpp"
#include "grpcpp/completion_queue.h"
#include "grpcpp/security/server_credentials.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <memory>
#include <thread>

#include "grpcpp/server_context.h"
#include "grpcpp/support/async_unary_call.h"

namespace {

constexpr auto target = "localhost:50051";
constexpr auto listeningPort = "0.0.0.0:50051";

using Flag = cppcoro::single_consumer_event;

struct RouteServer {
public:
  ::route::RouteService::AsyncService service;
  ::std::unique_ptr<::grpc::ServerCompletionQueue> completionQueue;
  ::std::unique_ptr<grpc::Server> server;
  ::grpc::ServerContext serverContext;
};

struct GetFeatureResponder {
  using Request = cppcoro::task<::route::Point>;
  using Response = cppcoro::task<::route::Feature>;

  GetFeatureResponder(RouteServer &server)
      : serverContext{}, server{server}, responder{&serverContext} {
    server.service.RequestGetFeature(
        &serverContext, &point, &responder, server.completionQueue.get(),
        server.completionQueue.get(), &setPointFlag);
  }

  cppcoro::task<::route::Point> GetRequest() {
    co_await setPointFlag;
    co_return point;
  }

  cppcoro::task<void> SetResponse(::route::Feature feature) {
    responder.Finish(feature, ::grpc::Status::OK, &doneFlag);
    co_await doneFlag;
  }

public:
  ::grpc::ServerContext serverContext;

  ::route::Point point;
  ::Flag setPointFlag;

  ::Flag doneFlag;

  ::RouteServer &server;
  ::grpc::ServerAsyncResponseWriter<::route::Feature> responder;
};

auto BuildServer() {
  auto server = std::make_unique<::RouteServer>();
  ::grpc::ServerBuilder serverBuilder;

  server->completionQueue =
      serverBuilder
          .AddListeningPort(listeningPort, ::grpc::InsecureServerCredentials())
          .RegisterService(&server->service)
          .AddCompletionQueue();

  server->server = serverBuilder.BuildAndStart();

  return std::move(server);
}

struct RouteClient {
public:
  ::std::shared_ptr<::grpc::Channel> channel;
  ::std::unique_ptr<::route::RouteService::Stub> stub;
  ::grpc::ServerCompletionQueue *completionQueue;

public:
  ::cppcoro::task<::route::Feature> GetFeature(::route::Point point) {
    ::Flag flag;
    ::route::Feature feature;
    ::grpc::Status status;
    ::grpc::ClientContext clientContext;
    ::route::Point __point = point;

    const auto getFeatureReader =
        stub->AsyncGetFeature(&clientContext, __point, completionQueue);
    getFeatureReader->Finish(&feature, &status, &flag);

    co_await flag;

    co_return feature;
  }
};

auto BuildClient(::grpc::ServerCompletionQueue *completionQueue) {
  auto client = std::make_unique<RouteClient>();
  const auto creds = ::grpc::InsecureChannelCredentials();
  client->channel = ::grpc::CreateChannel(target, creds);
  client->stub = ::route::RouteService::NewStub(client->channel);
  client->completionQueue = completionQueue;
  return std::move(client);
}

cppcoro::generator<std::pair<bool, ::Flag *>>
poll(::grpc::CompletionQueue &cq) {
  void *nextTag;
  bool nextOk = false;
  while (cq.Next(&nextTag, &nextOk)) {
    co_yield std::make_pair(nextOk, static_cast<::Flag *>(nextTag));
  }
}

void LogMessage(const google::protobuf::Message &message,
                const std::string &tag) {
  std::string buffer;
  google::protobuf::util::MessageToJsonString(message, &buffer, {});
  std::cout << tag << buffer << std::endl;
}

} // namespace

int main() {
  const auto server = BuildServer();
  const auto completionQueue = server->completionQueue.get();
  const auto client = BuildClient(completionQueue);

  auto clientSequence = [&]() -> ::cppcoro::task<void> {
    std::vector<::cppcoro::task<void>> tasks;
    for (int lat = 0; lat < 10; ++lat) {
      for (int lon = 0; lon < 10; ++lon) {
        tasks.emplace_back([&](int a, int b) -> ::cppcoro::task<void> {
          ::route::Point point;
          point.set_latitude(a);
          point.set_longitude(b);
          LogMessage(point, "Sending request: ");
          const auto feature = co_await client->GetFeature(point);
          LogMessage(feature, "Received response: ");
        }(lat, lon));
      }
    }
    co_await ::cppcoro::when_all(std::move(tasks));
  };

  auto serverSequence = [&]() -> ::cppcoro::task<void> {
    int i = 100;
    while (i-- > 0) {
      GetFeatureResponder responder{*server};
      const auto request = co_await responder.GetRequest();
      LogMessage(request, "Received request: ");
      ::route::Feature f;
      f.mutable_location()->set_latitude(request.latitude());
      f.mutable_location()->set_longitude(request.longitude());
      f.set_name("Cool name");
      LogMessage(f, "Sending response: ");
      co_await responder.SetResponse(f);
    }
  };

  auto mainLoop = [&]() {
    for (const auto [ok, flag] : poll(*completionQueue)) {
      if (ok) {
        flag->set();
      } else {
        std::cerr << "NOK\n";
      }
    }
  };

  std::thread mainLoopThread{mainLoop};

  ::cppcoro::sync_wait(
      ::cppcoro::when_all_ready(clientSequence(), serverSequence()));
  mainLoopThread.join();
}