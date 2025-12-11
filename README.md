# FSM - High-Performance Finite State Machine

A modern, header-only C++17 finite state machine library designed for real-time applications.

[![ci](https://github.com/SergeyMakeev/FSM/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SergeyMakeev/FSM/actions/workflows/ci.yml)

## Features

- **Header-only**: Just include `fsm/fsm.h` and you're ready to go
- **High performance**: Optimized for minimal overhead with zero heap allocations
- **Type-safe**: Uses C++ enum classes for compile-time state validation
- **Flexible callbacks**: Supports both stateless and capturing lambdas via std::function
- **Configurable transition policy**: Choose between immediate chained transitions or single-step transitions
- **Fixed-size arrays**: All state data stored in compile-time arrays for excellent cache locality
- **Clean API**: Simple interface for easy state configuration

## Quick Start

### Basic Example

```cpp
#include "fsm/fsm.h"

// Define your states
enum class PlayerState { Idle, Running, Jumping, Count };

// Define your context data
struct PlayerData {
    float velocity;
    bool jumpPressed;
};

int main() {
    PlayerData data = {0.0f, false};
    
    // Create FSM with Immediate policy (allows chained transitions)
    Fsm<PlayerState, PlayerData, TransitionPolicy::Immediate> fsm(PlayerState::Idle, &data);
    
    // Configure states
    fsm.state(PlayerState::Idle)
        .onEnter([](PlayerData* ctx, double time) {
            ctx->velocity = 0.0f;
        })
        .onUpdate([](PlayerData* ctx, double time) {
            if (ctx->jumpPressed)
                return StateTransition::to(PlayerState::Jumping);
            return StateTransition::stay();
        });
    
    fsm.state(PlayerState::Jumping)
        .onEnter([](PlayerData* ctx, double time) {
            ctx->velocity = 10.0f;
        })
        .onUpdate([](PlayerData* ctx, double time) {
            ctx->velocity -= 9.8f * time;
            if (ctx->velocity <= 0)
                return StateTransition::to(PlayerState::Idle);
            return StateTransition::stay();
        });
    
    // Update every frame
    fsm.update(0.016); // 60 FPS
    
    return 0;
}
```

### Capturing Lambdas

The library supports capturing lambdas for convenience:

```cpp
int score = 0;

fsm.state(PlayerState::Running)
    .onEnter([&score](PlayerData* ctx, double time) {
        score += 10;  // Capture external variables
    });
```

### Transition Policies

The FSM requires you to choose a transition policy that controls how state changes happen during `update()`:

#### Immediate
Allows multiple state transitions in a single `update()` call. When a state returns a transition, the FSM immediately enters the new state and calls its `onUpdate()` in the same frame. This continues until a state returns `stay()` or the safety limit is reached.

```cpp
// Chained transitions - multiple states can execute in one update
Fsm<PlayerState, PlayerData, TransitionPolicy::Immediate> fsm(PlayerState::Idle, &data);
```

**Best for:** Logic where multi-step transitions should happen atomically (e.g., death → respawn → idle all in one frame).

#### SingleTransition
Only one state transition per `update()` call. When a state returns a transition, the FSM switches to the new state but does NOT call the new state's `onEnter()` or `onUpdate()` until the next `update()` call. This provides more predictable, step-by-step behavior.

```cpp
// Single-step transitions - easier to debug and reason about
Fsm<PlayerState, PlayerData, TransitionPolicy::SingleTransition> fsm(PlayerState::Idle, &data);
```

**Best for:** Complex state machines where you need fine-grained control over timing.

**Example comparison:**

```cpp
// With Immediate policy:
// - Single update(): Idle -> Running -> Jumping (if both transitions happen)
// - All onEnter and onUpdate callbacks execute in one frame

// With SingleTransition policy:
// - Update 1: Idle transitions to Running (Running not entered yet)
// - Update 2: Running enters and transitions to Jumping (Jumping not entered yet)
// - Update 3: Jumping enters and stays
```

## Building

### As a CMake Subproject

```cmake
add_subdirectory(path/to/FSM)
target_link_libraries(your_target PRIVATE FSM::FSM)
```

### Running Tests

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Debug
ctest -C Debug
```

## Requirements

- C++17 or later
- CMake 3.14+ (for building tests)

## API Overview

### TransitionPolicy

Controls how state transitions are processed:

- `TransitionPolicy::Immediate` - Allow multiple transitions per `update()`
- `TransitionPolicy::SingleTransition` - Only one transition per `update()`

### StateTransition

- `StateTransition::to(NewState)` - Transition to a different state
- `StateTransition::stay()` - Remain in the current state

### Fsm

Template parameters:
- `StateEnum` - Your enum class with states (must include a `Count` value)
- `ContextType` - Your custom data structure
- `Policy` - Transition policy (`TransitionPolicy::Immediate` or `TransitionPolicy::SingleTransition`)

Methods:
- `state(state)` - Configure callbacks for a state (returns StateConfiguration)
- `update(time)` - Update the FSM for the current frame
- `getCurrentState()` - Get the current state
- `getContext()` - Access the context data

Constants:
- `kMaxTransitionsPerFrame` - Safety limit to prevent infinite transition loops (256)

### StateConfiguration

- `.onEnter(callback)` - Called once when entering the state
- `.onUpdate(callback)` - Called every frame while in the state (returns StateTransition)
- `.onExit(callback)` - Called once when leaving the state

## License

See LICENSE file for details.

