#include <cstdio>

#include "HelloWorld.grpc.pb.h"
#include "HelloWorld.pb.h"
#include "grpcpp/security/server_credentials.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <memory>

#include "grpcpp/server_context.h"
#include "grpcpp/support/async_unary_call.h"
#include "requests/RequestContext.h"
#include "requests/Status.h"
#include "requests/states/GetFeatureAcceptResponse.h"
#include "requests/states/State.h"

namespace {

constexpr auto target = "localhost:50051";
constexpr auto listeningPort = "0.0.0.0:50051";

auto BuildGetFeatureRequest() {
  auto getFeaturePointArg = ::route::Point();
  getFeaturePointArg.set_latitude(10);
  getFeaturePointArg.set_longitude(10);
  return getFeaturePointArg;
}

struct RouteServer {
  ::route::RouteService::AsyncService service;
  ::std::unique_ptr<::grpc::ServerCompletionQueue> completionQueue;
  ::std::unique_ptr<grpc::Server> server;
  ::grpc::ServerContext serverContext;
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
  ::grpc::ClientContext clientContext;
  std::shared_ptr<::grpc::Channel> channel;
  std::unique_ptr<::route::RouteService::Stub> stub;
  ::grpc::ServerCompletionQueue *completionQueue;
};

auto BuildClient(::grpc::ServerCompletionQueue *completionQueue) {
  auto client = std::make_unique<RouteClient>();
  const auto creds = ::grpc::InsecureChannelCredentials();
  client->channel = ::grpc::CreateChannel(target, creds);
  client->stub = ::route::RouteService::NewStub(client->channel);
  client->completionQueue = completionQueue;
  return std::move(client);
}

void CallAsyncGetFeature(const ::route::Point &point, RouteClient &client) {

  const auto state = new ::requests::states::GetFeatureAcceptResponse();
  const auto requestContext = new ::requests::RequestContext(state);

  const auto getFeatureReader = client.stub->AsyncGetFeature(
      &client.clientContext, point, client.completionQueue);
  getFeatureReader->Finish(&state->feature, &state->status, requestContext);
}

struct Finisher : public ::requests::states::State {
  ::requests::Status handle(bool ok) override {
    return ::requests::Status::DONE;
  }

  const char *name() const override { return "Finisher"; }
};

struct GetFeatureResponseContext {

  GetFeatureResponseContext(::grpc::ServerContext *serverContext)
      : responder{std::make_unique<Responder>(serverContext)} {}

  using Responder = ::grpc::ServerAsyncResponseWriter<::route::Feature>;
  ::route::Point request;
  ::route::Feature response;
  std::unique_ptr<Responder> responder;
};

struct WaitDoneGetFeature : public ::requests::states::State {
  WaitDoneGetFeature(std::unique_ptr<GetFeatureResponseContext> responseContext)
      : responseContext(std::move(responseContext)) {}

  ::requests::Status handle(bool ok) override {
    std::cerr << "WaitDoneGetFeature::handle ok=" << ok << std::endl;
    return ::requests::Status::DONE;
  }

  const char *name() const override { return "WaitDoneGetFeature"; }

private:
  std::unique_ptr<GetFeatureResponseContext> responseContext;
};

struct ServerGetFeatureDelegate {
  using Response = std::pair<::grpc::Status, ::route::Feature>;
  using Request = ::route::Point;

  virtual Response OnGetFeature(const Request &) = 0;
  virtual ~ServerGetFeatureDelegate() = default;
};

struct PrintingServerGetFeatureDelegate : public ServerGetFeatureDelegate {
  Response OnGetFeature(const Request &request) override {
    std::cerr << "Received request lat=" << request.latitude()
              << " lon=" << request.longitude() << std::endl;
    Response response;

    std::get<::grpc::Status>(response) = ::grpc::Status::OK;
    std::get<::route::Feature>(response).set_name("Very Cool Feature");
    *std::get<::route::Feature>(response).mutable_location() = request;

    return response;
  }
};

struct WaitGetFeature : public ::requests::states::State {
  void start(RouteServer &server,
             std::unique_ptr<ServerGetFeatureDelegate> _delegate) {
    responseContext =
        std::make_unique<GetFeatureResponseContext>(&server.serverContext);
    delegate = std::move(_delegate);
    const auto completionQueue = server.completionQueue.get();
    server.service.RequestGetFeature(
        &server.serverContext, &responseContext->request,
        responseContext->responder.get(), completionQueue, completionQueue,
        GetContext());
  }

  ::requests::Status handle(bool ok) override {
    if (ok) {
      const auto response = delegate->OnGetFeature(responseContext->request);

      responseContext->response = std::get<::route::Feature>(response);
      responseContext->responder->Finish(responseContext->response,
                                         std::get<::grpc::Status>(response),
                                         GetContext());
      const auto finisher = new WaitDoneGetFeature{std::move(responseContext)};
      GetContext()->TransitionTo(finisher);
      return ::requests::Status::CONTINUE;
    } else {
      return ::requests::Status::DONE;
    }
  }

  const char *name() const override { return "WaitGetFeature"; }

private:
  std::unique_ptr<GetFeatureResponseContext> responseContext;
  std::unique_ptr<ServerGetFeatureDelegate> delegate;
};

void RespondAsyncGetFeature(RouteServer &server) {
  const auto state = new WaitGetFeature();
  const auto context = new ::requests::RequestContext(state);
  state->start(server, std::make_unique<PrintingServerGetFeatureDelegate>());
}

} // namespace

int main() {

  const std::vector<std::string> lines{"Hello", "World"};
  for (std::string line : lines) {
    std::cout << line << std::endl;
  }

  const auto server = BuildServer();
  const auto completionQueue = server->completionQueue.get();
  const auto client = BuildClient(completionQueue);

  RespondAsyncGetFeature(*server);
  ::route::Point point;
  point.set_latitude(10);
  point.set_longitude(20);

  CallAsyncGetFeature(point, *client);

  void *nextTag;
  bool nextOk = false;
  while (completionQueue->Next(&nextTag, &nextOk)) {
    std::cout << "loop nextTag=" << (void *)nextTag << " nextOk=" << nextOk
              << "\n";
    auto context = static_cast<::requests::RequestContext *>(nextTag);

    switch (context->Handle(nextOk)) {
    case ::requests::Status::CONTINUE:
      break;

    case ::requests::Status::DONE:
      delete context;
      break;
    }
    std::cout << "loop done\n";
  }
}