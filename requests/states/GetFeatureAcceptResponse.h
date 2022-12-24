#pragma once

#include "HelloWorld.pb.h"
#include "requests/Status.h"
#include "requests/states/State.h"
#include <google/protobuf/util/json_util.h>
#include <grpcpp/support/status.h>
// #include <protobuf/

namespace requests {
namespace states {
struct GetFeatureAcceptResponse : public State {
  ::route::Feature feature;
  ::grpc::Status status;

  GetFeatureAcceptResponse() {}

  const char *name() const override { return "GetFeatureAcceptResponse"; }

  ::requests::Status handle(bool ok) override {
    if (!ok) {
      std::cerr << "received nok from mainloop\n";
    } else if (!status.ok()) {
      std::cerr << "Error Message:" << status.error_message() << "\n";
      std::cerr << "Error Details:" << status.error_details() << "\n";
    } else {
      std::string buffer;
      google::protobuf::util::MessageToJsonString(feature, &buffer, {});
      std::cerr << "Received feature: " << buffer << std::endl;
    }

    return ::requests::Status::DONE;
  }
};

} // namespace states
} // namespace requests