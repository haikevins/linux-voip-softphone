# Call State Machine — Phase 1

## Purpose

The call state machine is the first production-oriented C++ component in the project. Its job is
to make call lifecycle transitions explicit, testable, and independent from PJSIP or Qt.

At Phase 1 there is no real SIP traffic yet. Events are simulated in C++ so that the state model
can be verified before networking is introduced.

## States

```text
Idle
  |
  | RegisterRequested
  v
Registering
  |       \
  |        \ RegistrationFailed
  v         v
Ready     Failed
  |         |
  |         | ResetRequested
  |         v
  |        Idle
  |
  +-----------------------+
  |                       |
  | OutgoingCallRequested | IncomingCallReceived
  v                       v
Calling ---------------> Ringing
  |                         |
  |                         |
  +-------> Connected <-----+
                |
                | HangupRequested
                v
           Terminating
                |
                | CallEnded
                v
              Ready
```

The implementation also allows failure transitions from active call states.

## Why `enum class`

`CallState` uses `enum class` rather than an unscoped `enum` because it provides scoped names and
stronger type safety:

```cpp
voip::CallState::Connected
```

A `CallState` value is not implicitly converted to an integer, which helps prevent accidental use
of state values as unrelated numeric values.

## Events with `std::variant`

`Event` is a `std::variant` of concrete event types such as:

```text
RegisterRequested
RegistrationSucceeded
OutgoingCallRequested
IncomingCallReceived
RemoteRinging
CallConnected
HangupRequested
CallEnded
CallFailed
```

This creates a closed set of event alternatives. `std::visit` then performs type-safe dispatch.

## Optional values

The state machine uses `std::optional<std::string>` for values that may not exist:

```text
peer
last_error
```

For example, an idle state does not require a peer address, so an empty optional is clearer than
using an empty string as a magic value.

## STL containers

A `std::vector<CallState>` stores transition history. A vector is suitable because history is
append-oriented, contiguous, and normally iterated sequentially.

## Interface and dependency injection

`StateChangeSink` is an interface:

```cpp
class StateChangeSink {
public:
    virtual ~StateChangeSink() = default;
    virtual void on_state_changed(StateTransition transition) = 0;
};
```

`CallStateMachine` receives a `StateChangeSink&` through its constructor. This is constructor
injection: the state machine depends on an abstraction rather than a concrete console or UI
implementation.

Production code can use a Qt-oriented sink later, while tests can use a small recording fake.

## Invalid transitions

Invalid transitions return `false` and leave state unchanged. Example:

```text
Idle -> Connected
```

is rejected because a call cannot become connected before registration/call setup steps occur.

The behavior is covered by `tests/unit/test_call_state.cpp` and
`tests/unit/test_call_state_machine.cpp`.
