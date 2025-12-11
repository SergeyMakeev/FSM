#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

/*
**State Transition Policy**

Controls how many state transitions can occur in a single update() call.

This choice affects how the FSM behaves when states immediately transition to other states.
In some scenarios, you want instant chaining (player dies -> respawn -> idle all in one frame).
In others, you want predictable single-step transitions for easier debugging and control.

**Immediate**: Allows chained transitions in one update.
When a state transitions, the new state's onEnter and onUpdate execute immediately in the same frame.
This continues until a state returns stay() or the safety limit is reached.
Best for: logic where multi-step transitions should happen atomically.

**SingleTransition**: Only one transition per update.
When a state transitions, we switch immediately but don't enter the new state until next update().
This provides step-by-step execution that's easier to reason about and debug.
Best for: complex state machines where you need fine-grained control over timing.
*/
enum class TransitionPolicy
{
    Immediate,
    SingleTransition
};

/*
**State Transition Decision**

Return type for state update callbacks - tells the FSM whether to stay or transition.

This design uses the Named Constructor idiom to prevent users from creating invalid transitions.
The private constructor ensures you can only create transitions through to() or stay(),
which makes the API self-documenting and prevents accidental misuse.

Usage:
  return StateTransition::to(MyState::Running);   // Switch to Running state
  return StateTransition::stay();                 // Remain in current state

The internal representation uses a uint64_t to store the target state value,
which allows any enum type (regardless of underlying type) to be safely converted.
The bool flag provides a fast check for the common "stay" case without comparing state values.
*/
class StateTransition
{
  public:
    template <typename StateType> [[nodiscard]] static constexpr StateTransition to(StateType newState) noexcept
    {
        static_assert(std::is_enum_v<StateType>, "State must be an enum type");
        return StateTransition(static_cast<uint64_t>(newState), false);
    }

    [[nodiscard]] static constexpr StateTransition stay() noexcept { return StateTransition(0, true); }

  private:
    template <typename, typename, TransitionPolicy> friend class Fsm;

    uint64_t targetStateValue;
    bool shouldRemainInCurrentState;

    // Private constructor forces use of named constructors to() and stay()
    explicit constexpr StateTransition(uint64_t stateValue, bool stayInState) noexcept
        : targetStateValue(stateValue)
        , shouldRemainInCurrentState(stayInState)
    {
    }
};

/*
**High-Performance Finite State Machine**

A zero-allocation, header-only FSM designed for real-time systems.

**Design Philosophy:**
This FSM prioritizes performance and simplicity over flexibility. It uses fixed-size arrays
instead of maps/vectors to avoid heap allocations and cache misses. State callbacks are stored
in a compile-time-sized array indexed directly by the enum value for O(1) lookup.

**Template Parameters:**
- StateEnum: Your enum class defining all states. MUST end with a 'Count' sentinel value.
- ContextType: Your data struct. The FSM doesn't own this - you control the lifetime.
- Policy: TransitionPolicy controlling single-step vs chained transitions (Immediate or SingleTransition).

**Performance Characteristics:**
- State transitions: O(1)
- Memory: Fixed at compile time, no heap allocations
- Callbacks: std::function allows both stateless and capturing lambdas
  Modern compilers optimize stateless lambdas to zero overhead

**Usage Example:**
  enum class State { Idle, Running, Jumping, Count };
  struct Data { float velocity; };

  Fsm<State, Data, TransitionPolicy::Immediate> fsm(State::Idle, &data);
  // Or with single-step transitions:
  Fsm<State, Data, TransitionPolicy::SingleTransition> fsm(State::Idle, &data);
*/
template <typename StateEnum, typename ContextType, TransitionPolicy Policy> class Fsm
{
    static_assert(std::is_enum_v<StateEnum>, "StateEnum must be an enum or enum class");

  public:
    using TimeValue = double;

    // Loop until we find a state that wants to stay or has no update callback
    // The limit prevents infinite loops from buggy state logic
    static constexpr size_t kMaxTransitionsPerFrame = 256;

  private:
    // Convert enum to array index for O(1) callback lookup
    [[nodiscard]] constexpr std::size_t getStateIndex(StateEnum state) const noexcept
    {
        using UnderlyingType = std::underlying_type_t<StateEnum>;
        return static_cast<std::size_t>(static_cast<UnderlyingType>(state));
    }

    /*
    **State Callback Storage**

    Each state has three optional callbacks: onEnter, onUpdate, and onExit.

    We use std::function to support both stateless and capturing lambdas.
    This adds some overhead compared to function pointers, but provides essential flexibility.
    Modern compilers optimize stateless lambdas to have minimal cost.

    The alternative would be templates (like std::visit), but that would require all states
    to have the same callback types, which is too restrictive for a general FSM.
    We prioritize ease of use over the last bit of performance here.
    */
    struct StateCallbacks
    {
        using EnterCallback = std::function<void(ContextType*, TimeValue)>;
        using UpdateCallback = std::function<StateTransition(ContextType*, TimeValue)>;
        using ExitCallback = std::function<void(ContextType*, TimeValue)>;

        EnterCallback onEnter;
        UpdateCallback onUpdate;
        ExitCallback onExit;
    };

  public:
    /*

    This helper class provides a chainable interface for configuring state callbacks.
    It holds a reference to the actual callback storage and allows method chaining.

    The class is designed to be returned by value and then chained, like:
      fsm.state(State::Idle)
         .onEnter([](Data* ctx, double time) { ctx->counter = 0; })
         .onUpdate([](Data* ctx, double time) { return StateTransition::stay(); });

    The reference member makes this naturally non-copyable but moveable,
    which is perfect for the return-by-value pattern we use.
    This prevents accidental copies while still allowing the fluent syntax.

    All callbacks are optional. States without onUpdate will never transition.
    */
    class StateConfiguration
    {
      public:
        template <typename CallbackFunction> StateConfiguration& onEnter(CallbackFunction callback)
        {
            callbacks.onEnter = callback;
            return *this;
        }

        template <typename CallbackFunction> StateConfiguration& onUpdate(CallbackFunction callback)
        {
            callbacks.onUpdate = callback;
            return *this;
        }

        template <typename CallbackFunction> StateConfiguration& onExit(CallbackFunction callback)
        {
            callbacks.onExit = callback;
            return *this;
        }

      private:
        friend class Fsm;
        StateCallbacks& callbacks;

        explicit StateConfiguration(StateCallbacks& cbs)
            : callbacks(cbs)
        {
        }
    };

  public:
    static constexpr std::size_t TotalStateCount = static_cast<std::size_t>(StateEnum::Count);

    // We require the enum to have a Count sentinel value for compile-time array sizing
    // The 255 limit is arbitrary but reasonable - if you need more states, you probably
    // need a different architecture (hierarchical FSM, behavior trees, etc.)
    static_assert(TotalStateCount > 0 && TotalStateCount < 256,
                  "You must have between 1 and 255 states. Did you forget the 'Count' value in your enum?");

    /*
    **Constructor**

    Creates a new FSM starting in the specified initial state.
    The state's onEnter callback will be called on the first update().

    The context pointer is optional - pass nullptr if your state logic doesn't need external data.
    The FSM does NOT take ownership of the context; you're responsible for its lifetime.
    This design gives you full control over memory management and layout.

    Example:
      Data data;
      Fsm<State, Data, TransitionPolicy::Immediate> fsm(State::Idle, &data);
    */
    Fsm(StateEnum initialState, ContextType* contextData = nullptr)
        : contextPointer(contextData)
        , currentActiveState(initialState)
        , lastEnteredState(StateEnum::Count)
        , allStateCallbacks{}
    {
        // Verify initial state is valid at runtime
        // We use Count as a sentinel/invalid value for lastEnteredState,
        // so initialState must be less than Count
        assert(initialState < StateEnum::Count && "Initial state is invalid!");
    }

    Fsm(const Fsm&) = delete;
    Fsm& operator=(const Fsm&) = delete;
    Fsm(Fsm&&) = default;
    Fsm& operator=(Fsm&&) = default;

    /*
    **Configure a state's callbacks**

    Returns a configuration object for setting up onEnter, onUpdate, and onExit.
    Must be called BEFORE the first update() - we don't allow runtime reconfiguration

    Example:
      fsm.state(State::Active)
         .onEnter([](Data* ctx, double time) { ctx->counter = 0; })
         .onUpdate([](Data* ctx, double time) { return StateTransition::stay(); });
    */
    [[nodiscard]] StateConfiguration state(StateEnum state) noexcept
    {
        // Validate the state enum value is in range
        assert(state < StateEnum::Count && "Invalid state - check your enum!");
        // Prevent reconfiguration after FSM has started running
        assert(lastEnteredState == StateEnum::Count && "Cannot configure states after calling update() - configure all states first!");
        return StateConfiguration(allStateCallbacks[getStateIndex(state)]);
    }

    [[nodiscard]] StateEnum getCurrentState() const noexcept { return currentActiveState; }
    [[nodiscard]] ContextType* getContext() noexcept { return contextPointer; }
    [[nodiscard]] const ContextType* getContext() const noexcept { return contextPointer; }

    /*
    **Update the FSM**

    The behavior depends on which TransitionPolicy you chose:

    **With Immediate Policy:**
    Allows chained transitions in a single update. When a state transitions, the new state
    is entered and updated immediately in the same frame. This continues until a state
    returns stay() or we hit the safety limit.

    Flow: onEnter (if needed) -> onUpdate -> (if transitioning: onExit -> switch) -> loop

    **With SingleTransition Policy:**
    Only one transition per update. When a state transitions, we switch immediately but
    don't enter the new state until the next update() call. This provides step-by-step
    execution that's easier to debug.

    Flow: onEnter (if needed) -> onUpdate -> (if transitioning: onExit -> switch) -> done

    The safety limit of kMaxTransitionsPerFrame transitions exists to catch infinite loops in your state logic.
    If you hit this, you have a bug (e.g., StateA -> StateB -> StateA -> StateB...).
    */
    inline void update(TimeValue currentTime)
    {
        // Sanity check - this should never happen unless there's memory corruption
        if (currentActiveState >= StateEnum::Count)
        {
            assert(false && "State machine is in an invalid state! This should never happen.");
            return;
        }

        if constexpr (Policy == TransitionPolicy::Immediate)
        {
            for (size_t transitionCount = 0; transitionCount < kMaxTransitionsPerFrame; ++transitionCount)
            {
                if (!processStateStep(currentTime))
                {
                    return;
                }
            }

            // we transitioned too many times in one frame
            assert(false && "State machine exceeded maximum transitions per frame! Do you have an infinite loop in the state logic?");
        }
        else // TransitionPolicy::SingleTransition
        {
            // Process one step and stop, even if a transition occurred
            // The new state will be entered on the next update() call
            processStateStep(currentTime);
        }
    }

  private:
    /*
    **Process one state step**

    This is the core FSM logic extracted into a helper to avoid code duplication
    between the two transition policies.

    Steps:
    1. If we haven't entered this state yet, call onEnter
    2. Call onUpdate to get the state's transition decision
    3. If staying, return false
    4. If transitioning, validate the target, call onExit, switch states, return true

    The return value tells the caller whether a transition occurred, which determines
    whether to continue looping (Immediate policy) or stop (SingleTransition policy).

    States without an onUpdate callback are terminal - they can never transition out.
    This is useful for "game over" or other final states.
    */
    inline bool processStateStep(TimeValue currentTime)
    {
        StateCallbacks& currentCallbacks = allStateCallbacks[getStateIndex(currentActiveState)];

        // Check if we need to call onEnter for this state
        // lastEnteredState tracks which state we last called onEnter for
        if (currentActiveState != lastEnteredState)
        {
            if (currentCallbacks.onEnter)
            {
                currentCallbacks.onEnter(contextPointer, currentTime);
            }
            lastEnteredState = currentActiveState;
        }

        // States without onUpdate are terminal - they can never transition
        if (!currentCallbacks.onUpdate)
        {
            return false;
        }

        const StateTransition transitionDecision = currentCallbacks.onUpdate(contextPointer, currentTime);

        if (transitionDecision.shouldRemainInCurrentState)
        {
            return false;
        }

        const StateEnum nextState = static_cast<StateEnum>(transitionDecision.targetStateValue);

        // Ignore invalid transitions - treat them as stay()
        // This includes transitioning to the same state or to an out-of-range state
        if (nextState == currentActiveState || nextState >= StateEnum::Count)
        {
            return false;
        }

        // Execute the transition
        if (currentCallbacks.onExit)
        {
            currentCallbacks.onExit(contextPointer, currentTime);
        }

        currentActiveState = nextState;
        return true;
    }

    ContextType* contextPointer;
    StateEnum currentActiveState;

    // Dual-purpose: tracks which state we last called onEnter for,
    // AND serves as a "has FSM started" flag (equals Count before first update)
    StateEnum lastEnteredState;

    // Fixed-size array indexed by state enum value for O(1) callback lookup
    // This avoids heap allocations and provides excellent cache locality
    StateCallbacks allStateCallbacks[TotalStateCount];
};
