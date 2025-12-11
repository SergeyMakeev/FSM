#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

/// @brief Represents the result of a state update - either stay or switch to a new state
/// @details ONLY use the static factory methods: switchTo() or stayInCurrent()
///
/// Example:
///   return StateTransition::switchTo(MyState::Running);
///   return StateTransition::stayInCurrent();
class StateTransition
{
  public:
    /// @brief Transition to a different state
    /// @param newState The state to transition to
    /// @return StateTransition that will switch to the specified state
    template <typename StateType> [[nodiscard]] static constexpr StateTransition switchTo(StateType newState) noexcept
    {
        static_assert(std::is_enum_v<StateType>, "State must be an enum type");
        return StateTransition(static_cast<uint64_t>(newState), false);
    }

    /// @brief Stay in the current state (do not transition)
    /// @return StateTransition that keeps the FSM in its current state
    [[nodiscard]] static constexpr StateTransition stayInCurrent() noexcept { return StateTransition(0, true); }

  private:
    // Allow FSM to access internals
    template <typename, typename> friend class StateMachine;

    uint64_t targetStateValue;
    bool shouldRemainInCurrentState;

    // Private constructor - users must use switchTo() or stayInCurrent()
    explicit constexpr StateTransition(uint64_t stateValue, bool stayInState) noexcept
        : targetStateValue(stateValue)
        , shouldRemainInCurrentState(stayInState)
    {
    }
};

/// @brief High-performance Finite State Machine
/// @tparam StateEnum Your enum class with states (MUST include a 'Count' value at the end)
/// @tparam ContextType Your custom data structure to store state and game data
///
/// Example:
///   enum class PlayerState { Idle, Running, Jumping, Count };
///   struct PlayerData { float speed; int health; };
///   StateMachine<PlayerState, PlayerData> fsm(PlayerState::Idle, &data);
template <typename StateEnum, typename ContextType> class StateMachine
{
    static_assert(std::is_enum_v<StateEnum>, "StateEnum must be an enum or enum class");

  public:
    using TimeValue = double;

  private:
    // Convert state enum to array index
    [[nodiscard]] constexpr std::size_t getStateIndex(StateEnum state) const noexcept
    {
        using UnderlyingType = std::underlying_type_t<StateEnum>;
        return static_cast<std::size_t>(static_cast<UnderlyingType>(state));
    }

    /// @brief Holds the three callbacks for each state (enter, update, exit)
    /// @note Uses std::function to support both stateless and capturing lambdas
    ///       Modern implementations optimize stateless lambdas to avoid overhead
    struct StateCallbacks
    {
        // Called once when entering this state
        using EnterCallback = std::function<void(ContextType*, TimeValue)>;

        // Called every frame while in this state - returns transition decision
        using UpdateCallback = std::function<StateTransition(ContextType*, TimeValue)>;

        // Called once when leaving this state
        using ExitCallback = std::function<void(ContextType*, TimeValue)>;

        EnterCallback onEnter;
        UpdateCallback onUpdate;
        ExitCallback onExit;
    };

  public:
    /// @brief Helper for configuring a state's callbacks with a fluent/chainable API
    ///
    /// Note: Supports both stateless and capturing lambdas via std::function.
    ///       Stateless lambdas are optimized by the compiler with no overhead.
    ///       For best performance, prefer storing data in ContextType when possible.
    ///
    /// Example:
    ///   fsm.configureState(MyState::Idle)
    ///      .onEnter([](MyContext* ctx, double time) { ctx->counter = 0; })
    ///      .onUpdate([](MyContext* ctx, double time) {
    ///          return StateTransition::stayInCurrent();
    ///      });
    class StateConfiguration
    {
      public:
        /// @brief Set what happens when ENTERING this state (called once)
        /// @param callback Function with signature: void(ContextType*, TimeValue)
        /// @note Supports both stateless and capturing lambdas
        template <typename CallbackFunction> StateConfiguration& onEnter(CallbackFunction callback)
        {
            callbacks.onEnter = callback;
            return *this;
        }

        /// @brief Set what happens every frame while IN this state
        /// @param callback Function with signature: StateTransition(ContextType*, TimeValue)
        /// @note Must return StateTransition::switchTo() or stayInCurrent()
        /// @note Supports both stateless and capturing lambdas
        template <typename CallbackFunction> StateConfiguration& onUpdate(CallbackFunction callback)
        {
            callbacks.onUpdate = callback;
            return *this;
        }

        /// @brief Set what happens when LEAVING this state (called once)
        /// @param callback Function with signature: void(ContextType*, TimeValue)
        /// @note Supports both stateless and capturing lambdas
        template <typename CallbackFunction> StateConfiguration& onExit(CallbackFunction callback)
        {
            callbacks.onExit = callback;
            return *this;
        }

      private:
        friend class StateMachine;
        StateCallbacks& callbacks;

        explicit StateConfiguration(StateCallbacks& cbs)
            : callbacks(cbs)
        {
        }

        // Note: The reference member makes this class naturally non-copyable
        // but moveable, which is exactly what we want for the return-by-value pattern
    };

  public:
    static constexpr std::size_t TotalStateCount = static_cast<std::size_t>(StateEnum::Count);
    static_assert(TotalStateCount > 0 && TotalStateCount < 256,
                  "You must have between 1 and 255 states. Did you forget the 'Count' value in your enum?");

    /// @brief Create a new state machine
    /// @param initialState Which state to start in (onEnter will be called on first update)
    /// @param contextData Pointer to your game/context data (optional, can be nullptr)
    ///
    /// Example:
    ///   PlayerData data;
    ///   StateMachine<PlayerState, PlayerData> fsm(PlayerState::Idle, &data);
    StateMachine(StateEnum initialState, ContextType* contextData = nullptr)
        : contextPointer(contextData)
        , currentActiveState(initialState)
        , lastEnteredState(StateEnum::Count) // Initialize to invalid state to trigger onEnter on first update
        , allStateCallbacks{}                 // Value-initialize all callbacks (std::function default = empty)
    {
        assert(initialState < StateEnum::Count && "Initial state is invalid!");
    }

    // Prevent copying (state machines should not be copied accidentally)
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    // Allow moving if needed (not noexcept due to std::function)
    StateMachine(StateMachine&&) = default;
    StateMachine& operator=(StateMachine&&) = default;

    /// @brief Set up callbacks for a specific state
    /// @param state Which state to configure
    /// @return StateConfiguration object for chaining .onEnter().onUpdate().onExit()
    ///
    /// Example:
    ///   fsm.state(PlayerState::Jumping)
    ///      .onEnter([](PlayerData* ctx, double time) { ctx->velocity = 10; })
    ///      .onUpdate([](PlayerData* ctx, double time) {
    ///          return StateTransition::stayInCurrent();
    ///      });
    [[nodiscard]] StateConfiguration state(StateEnum state) noexcept
    {
        assert(state < StateEnum::Count && "Invalid state - check your enum!");
        assert(!hasStartedUpdating && "Cannot configure states after calling update() - configure all states first!");
        return StateConfiguration(allStateCallbacks[getStateIndex(state)]);
    }

    /// @brief Get which state the machine is currently in
    /// @return The current state enum value
    [[nodiscard]] StateEnum getCurrentState() const noexcept { return currentActiveState; }

    /// @brief Access your context/game data
    /// @return Pointer to your context data (might be nullptr if you didn't provide one)
    [[nodiscard]] ContextType* getContext() noexcept { return contextPointer; }

    /// @brief Access your context/game data (const version)
    [[nodiscard]] const ContextType* getContext() const noexcept { return contextPointer; }

    /// @brief Update the state machine for this frame
    /// @param currentTime The current game/world time (passed to your callbacks)
    ///
    /// Call this once per frame/tick. It will:
    /// 1. Call onEnter() if this is the first update or if we just transitioned
    /// 2. Call onUpdate() which returns whether to stay or switch states
    /// 3. If switching: call onExit(), change state, call new state's onEnter()
    ///
    /// Note: Supports instant chained transitions (can switch through multiple states in one update)
    inline void update(TimeValue currentTime)
    {
        hasStartedUpdating = true;

        // Safety check: make sure state is valid
        if (currentActiveState >= StateEnum::Count)
        {
            assert(false && "State machine is in an invalid state! This should never happen.");
            return;
        }

        // Prevent infinite loops (e.g., StateA -> StateB -> StateA -> StateB...)
        // 256 transitions in one frame is definitely a bug in your state logic
        constexpr size_t MaxTransitionsPerFrame = 256;

        for (size_t transitionCount = 0; transitionCount < MaxTransitionsPerFrame; ++transitionCount)
        {
            // Get the callbacks for our current state
            StateCallbacks& currentCallbacks = allStateCallbacks[getStateIndex(currentActiveState)];

            // If we haven't entered this state yet, call onEnter
            if (currentActiveState != lastEnteredState)
            {
                if (currentCallbacks.onEnter)
                {
                    currentCallbacks.onEnter(contextPointer, currentTime);
                }
                lastEnteredState = currentActiveState;
            }

            // If there's no update callback, this state can't transition, so we're done
            if (!currentCallbacks.onUpdate)
            {
                return;
            }

            // Run the state's update logic - it will tell us whether to stay or switch
            const StateTransition transitionDecision = currentCallbacks.onUpdate(contextPointer, currentTime);

            // Should we stay in the current state?
            if (transitionDecision.shouldRemainInCurrentState)
            {
                return; // Yes, we're done for this frame
            }

            // We're switching to a new state!
            const StateEnum nextState = static_cast<StateEnum>(transitionDecision.targetStateValue);

            // Validate the transition makes sense
            if (nextState == currentActiveState || nextState >= StateEnum::Count)
            {
                // Trying to transition to same state or invalid state - ignore and stay
                return;
            }

            // Execute the transition: Exit old state -> Switch -> Enter new state
            if (currentCallbacks.onExit)
            {
                currentCallbacks.onExit(contextPointer, currentTime);
            }

            currentActiveState = nextState;

            // Loop continues to handle the new state - onEnter will be called on next iteration
            // This allows instant chained transitions (StateA -> StateB -> StateC in one frame)
        }

        // If we get here, we did 256+ transitions in one update - that's definitely wrong!
        assert(false && "State machine exceeded maximum transitions per frame! You have an infinite loop in your state logic.");
    }

  private:
    ContextType* contextPointer;
    StateEnum currentActiveState;
    StateEnum lastEnteredState;                        // Track which state we last called onEnter for
    bool hasStartedUpdating = false;                   // Prevent configuring states after update() is called
    StateCallbacks allStateCallbacks[TotalStateCount]; // Array of callbacks, one per state
};

// Backwards compatibility: allow using "Fsm" as the class name
template <typename StateEnum, typename ContextType> using Fsm = StateMachine<StateEnum, ContextType>;
