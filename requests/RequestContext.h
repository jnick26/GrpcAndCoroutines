#pragma once

#include "Status.h"
#include "states/State.h"

namespace requests {

struct RequestContext {
  RequestContext(::requests::states::State *state);
  ~RequestContext();
  void TransitionTo(::requests::states::State *state);
  ::requests::Status Handle(bool ok);

private:
  ::requests::states::State *_state;
};
} // namespace requests