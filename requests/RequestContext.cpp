#include "RequestContext.h"
#include "requests/Status.h"
#include <iostream>
namespace requests {

RequestContext::RequestContext(::requests::states::State *state)
    : _state{nullptr} {
  TransitionTo(state);
}

RequestContext::~RequestContext() { delete _state; }

void RequestContext::TransitionTo(::requests::states::State *state) {
  std::cerr << "RequestContext(" << (void *)this << ") TransitionTo "
            << state->name() << "\n";

  if (_state != nullptr) {
    delete _state;
  }

  _state = state;
  _state->SetContext(this);
}

::requests::Status RequestContext::Handle(bool ok) {
  return _state->handle(ok);
}

} // namespace requests