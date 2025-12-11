#include "fsm/fsm.h"
#include <gtest/gtest.h>

// Test enum for a simple state machine
enum class TestState
{
    Idle,
    Running,
    Jumping,
    Count
};

// Test context to store data during state transitions
struct TestContext
{
    int enterCount = 0;
    int updateCount = 0;
    int exitCount = 0;
    double lastTime = 0.0;
    bool shouldTransition = false;
    TestState targetState = TestState::Idle;

    // For tracking updates per state
    int idleUpdates = 0;
    int runningUpdates = 0;
    int jumpingUpdates = 0;
};

// Test: Creating a state machine with initial state
TEST(FSMTest, CanCreateStateMachine)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(fsm.getContext(), &context);
}

// Test: Getting context data
TEST(FSMTest, CanAccessContext)
{
    TestContext context;
    context.enterCount = 42;

    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    EXPECT_EQ(fsm.getContext()->enterCount, 42);
}

// Test: State machine calls onEnter on first update
TEST(FSMTest, CallsOnEnterOnFirstUpdate)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onEnter(
            [](TestContext* ctx, double time)
            {
                ctx->enterCount++;
                ctx->lastTime = time;
            })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::stayInCurrent(); });

    EXPECT_EQ(context.enterCount, 0);

    fsm.update(1.0);

    EXPECT_EQ(context.enterCount, 1);
    EXPECT_EQ(context.lastTime, 1.0);
}

// Test: onUpdate is called every frame
TEST(FSMTest, CallsOnUpdateEveryFrame)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->updateCount++;
                return StateTransition::stayInCurrent();
            });

    fsm.update(1.0);
    fsm.update(2.0);
    fsm.update(3.0);

    EXPECT_EQ(context.updateCount, 3);
}

// Test: State transitions work correctly
TEST(FSMTest, CanTransitionBetweenStates)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    // Configure Idle state to transition to Running
    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                if (ctx->shouldTransition)
                {
                    return StateTransition::switchTo(TestState::Running);
                }
                return StateTransition::stayInCurrent();
            });

    // Configure Running state
    fsm.state(TestState::Running).onEnter([](TestContext* ctx, double time) { ctx->enterCount++; });

    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);

    // Update without triggering transition
    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);

    // Trigger transition
    context.shouldTransition = true;
    fsm.update(2.0);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.enterCount, 1);
}

// Test: onExit is called when leaving a state
TEST(FSMTest, CallsOnExitWhenLeavingState)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onExit(
            [](TestContext* ctx, double time)
            {
                ctx->exitCount++;
                ctx->lastTime = time;
            })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::switchTo(TestState::Running); });

    EXPECT_EQ(context.exitCount, 0);

    fsm.update(5.0);

    EXPECT_EQ(context.exitCount, 1);
    EXPECT_EQ(context.lastTime, 5.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
}

// Test: Full lifecycle of state transition (Exit -> Enter)
TEST(FSMTest, CompleteStateTransitionLifecycle)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onExit([](TestContext* ctx, double time) { ctx->exitCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::switchTo(TestState::Running); });

    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::stayInCurrent(); });

    // First update: Idle onEnter -> Idle onUpdate -> transition -> Idle onExit -> Running onEnter
    fsm.update(1.0);

    EXPECT_EQ(context.enterCount, 2); // Idle onEnter + Running onEnter
    EXPECT_EQ(context.exitCount, 1);  // Idle onExit
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
}

// Test: StateTransition::stayInCurrent() keeps the state unchanged
TEST(FSMTest, StayInCurrentKeepsState)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->updateCount++;
                return StateTransition::stayInCurrent();
            });

    fsm.update(1.0);
    fsm.update(2.0);
    fsm.update(3.0);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(context.updateCount, 3);
}

// Test: Chained transitions (immediate transitions)
TEST(FSMTest, SupportsChainedTransitions)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    // Idle immediately transitions to Running
    fsm.state(TestState::Idle)
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::switchTo(TestState::Running); });

    // Running immediately transitions to Jumping
    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::switchTo(TestState::Jumping); });

    // Jumping stays
    fsm.state(TestState::Jumping)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::stayInCurrent(); });

    // Single update should transition: Idle -> Running -> Jumping
    fsm.update(1.0);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.enterCount, 2); // Running and Jumping entered
}

// Test: State machine without context (nullptr)
TEST(FSMTest, CanWorkWithoutContext)
{
    StateMachine<TestState, TestContext> fsm(TestState::Idle, nullptr);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                // Context should be nullptr as we passed nullptr
                return StateTransition::stayInCurrent();
            });

    fsm.update(1.0);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
}

// Test: Multiple state configurations
TEST(FSMTest, CanConfigureMultipleStates)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->idleUpdates++;
                if (ctx->idleUpdates >= 2)
                    return StateTransition::switchTo(TestState::Running);
                return StateTransition::stayInCurrent();
            });

    fsm.state(TestState::Running)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->runningUpdates++;
                if (ctx->runningUpdates >= 2)
                    return StateTransition::switchTo(TestState::Jumping);
                return StateTransition::stayInCurrent();
            });

    fsm.state(TestState::Jumping)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->jumpingUpdates++;
                return StateTransition::stayInCurrent();
            });

    // Update 1: Stay in Idle (idleUpdates = 1)
    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(context.idleUpdates, 1);

    // Update 2: Transition Idle -> Running (idleUpdates = 2, then chained: runningUpdates = 1)
    fsm.update(2.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.idleUpdates, 2);
    EXPECT_EQ(context.runningUpdates, 1); // Chained transition called Running.onUpdate

    // Update 3: Transition Running -> Jumping (runningUpdates = 2, then chained: jumpingUpdates = 1)
    fsm.update(3.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.runningUpdates, 2);
    EXPECT_EQ(context.jumpingUpdates, 1); // Chained transition called Jumping.onUpdate

    // Update 4 & 5: Stay in Jumping
    fsm.update(4.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.jumpingUpdates, 2);

    fsm.update(5.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.jumpingUpdates, 3);
}

// Test: Lambdas with captures work (thanks to std::function)
TEST(FSMTest, SupportsCapturingLambdas)
{
    TestContext context;
    StateMachine<TestState, TestContext> fsm(TestState::Idle, &context);

    // External state that can be captured
    int externalCounter = 0;
    bool transitionTriggered = false;

    fsm.state(TestState::Idle)
        .onEnter(
            [&externalCounter](TestContext* ctx, double time)
            {
                externalCounter++; // Capture by reference works!
            })
        .onUpdate(
            [&transitionTriggered](TestContext* ctx, double time)
            {
                if (transitionTriggered)
                    return StateTransition::switchTo(TestState::Running);
                return StateTransition::stayInCurrent();
            });

    fsm.state(TestState::Running).onEnter([&externalCounter](TestContext* ctx, double time) { externalCounter += 10; });

    EXPECT_EQ(externalCounter, 0);

    // First update: Idle.onEnter is called
    fsm.update(1.0);
    EXPECT_EQ(externalCounter, 1);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);

    // Trigger transition via captured variable
    transitionTriggered = true;
    fsm.update(2.0);

    EXPECT_EQ(externalCounter, 11); // 1 from Idle.onEnter + 10 from Running.onEnter
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
}
