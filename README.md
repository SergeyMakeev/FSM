# FSM - High-Performance Finite State Machine

A modern, header-only C++17 finite state machine library designed for game development and real-time applications.

## Features

- **Header-only**: Just include `fsm/fsm.h` and you're ready to go
- **High performance**: Optimized for game loops with minimal overhead
- **Type-safe**: Uses C++ enum classes for compile-time state validation
- **Flexible callbacks**: Supports both stateless and capturing lambdas via std::function
- **Chained transitions**: Automatically handles instant state transitions in a single update
- **Zero heap allocations**: All state data stored in fixed arrays
- **Clean API**: Fluent interface for easy state configuration

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
    StateMachine<PlayerState, PlayerData> fsm(PlayerState::Idle, &data);
    
    // Configure states
    fsm.configureState(PlayerState::Idle)
        .onEnter([](PlayerData* ctx, double time) {
            ctx->velocity = 0.0f;
        })
        .onUpdate([](PlayerData* ctx, double time) {
            if (ctx->jumpPressed)
                return StateTransition::switchTo(PlayerState::Jumping);
            return StateTransition::stayInCurrent();
        });
    
    fsm.configureState(PlayerState::Jumping)
        .onEnter([](PlayerData* ctx, double time) {
            ctx->velocity = 10.0f;
        })
        .onUpdate([](PlayerData* ctx, double time) {
            ctx->velocity -= 9.8f * time;
            if (ctx->velocity <= 0)
                return StateTransition::switchTo(PlayerState::Idle);
            return StateTransition::stayInCurrent();
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

fsm.configureState(PlayerState::Running)
    .onEnter([&score](PlayerData* ctx, double time) {
        score += 10;  // Capture external variables
    });
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

### StateTransition

- `StateTransition::switchTo(NewState)` - Transition to a different state
- `StateTransition::stayInCurrent()` - Remain in the current state

### StateMachine

- `configureState(state)` - Configure callbacks for a state (returns StateConfiguration)
- `update(time)` - Update the FSM for the current frame
- `getCurrentState()` - Get the current state
- `getContext()` - Access the context data

### StateConfiguration

- `.onEnter(callback)` - Called once when entering the state
- `.onUpdate(callback)` - Called every frame while in the state (returns StateTransition)
- `.onExit(callback)` - Called once when leaving the state

## License

See LICENSE file for details.

