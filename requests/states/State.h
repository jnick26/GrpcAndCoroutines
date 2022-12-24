#pragma once

#include "requests/Status.h"

namespace requests {

struct RequestContext;

namespace states {

struct State {
public:
  friend RequestContext;

  virtual ~State() = default;
  virtual ::requests::Status handle(bool ok) = 0;
  virtual const char *name() const = 0;

protected:
  RequestContext *GetContext() const { return _context; }
  void SetContext(RequestContext *context) { _context = context; }

private:
  RequestContext *_context;
};
} // namespace states
} // namespace requests