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

// Test: SingleTransition policy only allows one transition per update
TEST(FSMSingleTransitionTest, OnlyOneTransitionPerUpdate)
{
    TestContext context;
    Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

    // Configure Idle to immediately transition to Running
    fsm.state(TestState::Idle)
        .onEnter([](TestContext* ctx, double time) { ctx->idleUpdates++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Running); });

    // Configure Running to immediately transition to Jumping
    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->runningUpdates++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Jumping); });

    // Configure Jumping to stay
    fsm.state(TestState::Jumping)
        .onEnter([](TestContext* ctx, double time) { ctx->jumpingUpdates++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::stay(); });

    // First update: Idle onEnter -> Idle onUpdate -> transition to Running (but don't enter yet)
    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.idleUpdates, 1);     // Idle was entered
    EXPECT_EQ(context.runningUpdates, 0);  // Running NOT entered yet
    EXPECT_EQ(context.jumpingUpdates, 0);

    // Second update: Running onEnter -> Running onUpdate -> transition to Jumping (but don't enter yet)
    fsm.update(2.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.idleUpdates, 1);
    EXPECT_EQ(context.runningUpdates, 1);  // Running was entered
    EXPECT_EQ(context.jumpingUpdates, 0);  // Jumping NOT entered yet

    // Third update: Jumping onEnter -> Jumping onUpdate -> stay
    fsm.update(3.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
    EXPECT_EQ(context.idleUpdates, 1);
    EXPECT_EQ(context.runningUpdates, 1);
    EXPECT_EQ(context.jumpingUpdates, 1);  // Jumping was entered
}

// Test: Compare Immediate vs SingleTransition behavior
TEST(FSMSingleTransitionTest, CompareWithImmediatePolicy)
{
    // Test with Immediate policy (default)
    {
        TestContext context;
        Fsm<TestState, TestContext, TransitionPolicy::Immediate> fsm(TestState::Idle, &context);

        fsm.state(TestState::Idle)
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Running); });

        fsm.state(TestState::Running)
            .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Jumping); });

        fsm.state(TestState::Jumping)
            .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::stay(); });

        // Single update transitions: Idle -> Running -> Jumping
        fsm.update(1.0);
        EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
        EXPECT_EQ(context.enterCount, 2); // Both Running and Jumping entered
    }

    // Test with SingleTransition policy
    {
        TestContext context;
        Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

        fsm.state(TestState::Idle)
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Running); });

        fsm.state(TestState::Running)
            .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Jumping); });

        fsm.state(TestState::Jumping)
            .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
            .onUpdate([](TestContext* ctx, double time) { return StateTransition::stay(); });

        // First update: Only transitions Idle -> Running
        fsm.update(1.0);
        EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
        EXPECT_EQ(context.enterCount, 0); // Running not entered yet

        // Second update: Only transitions Running -> Jumping
        fsm.update(2.0);
        EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
        EXPECT_EQ(context.enterCount, 1); // Only Running entered so far

        // Third update: Jumping is entered
        fsm.update(3.0);
        EXPECT_EQ(fsm.getCurrentState(), TestState::Jumping);
        EXPECT_EQ(context.enterCount, 2); // Now Jumping is entered too
    }
}

// Test: SingleTransition policy with conditional transitions
TEST(FSMSingleTransitionTest, ConditionalTransitions)
{
    TestContext context;
    Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->idleUpdates++;
                if (ctx->idleUpdates >= 3)
                    return StateTransition::to(TestState::Running);
                return StateTransition::stay();
            });

    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->runningUpdates++;
                return StateTransition::stay();
            });

    // Updates 1-2: Stay in Idle
    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(context.idleUpdates, 1);

    fsm.update(2.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(context.idleUpdates, 2);

    // Update 3: Transition to Running (but don't enter yet)
    fsm.update(3.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.idleUpdates, 3);
    EXPECT_EQ(context.enterCount, 0);     // Not entered yet
    EXPECT_EQ(context.runningUpdates, 0); // Not updated yet

    // Update 4: Enter Running and call its update
    fsm.update(4.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.enterCount, 1);     // Now entered
    EXPECT_EQ(context.runningUpdates, 1); // Now updated
}

// Test: SingleTransition policy calls onExit correctly
TEST(FSMSingleTransitionTest, OnExitCalledDuringTransition)
{
    TestContext context;
    Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onExit([](TestContext* ctx, double time) { ctx->exitCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Running); });

    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; })
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::stay(); });

    // First update: Idle onEnter -> Idle onUpdate -> Idle onExit -> switch to Running
    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.enterCount, 1); // Only Idle entered
    EXPECT_EQ(context.exitCount, 1);  // Idle exited during transition

    // Second update: Running onEnter -> Running onUpdate
    fsm.update(2.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.enterCount, 2); // Now Running entered too
    EXPECT_EQ(context.exitCount, 1);  // No additional exits
}

// Test: SingleTransition with stay() keeps state
TEST(FSMSingleTransitionTest, StayInCurrentKeepsState)
{
    TestContext context;
    Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate(
            [](TestContext* ctx, double time)
            {
                ctx->updateCount++;
                return StateTransition::stay();
            });

    fsm.update(1.0);
    fsm.update(2.0);
    fsm.update(3.0);

    EXPECT_EQ(fsm.getCurrentState(), TestState::Idle);
    EXPECT_EQ(context.updateCount, 3);
}

// Test: Fsm type alias works with SingleTransition policy
TEST(FSMSingleTransitionTest, TypeAliasWorks)
{
    TestContext context;
    Fsm<TestState, TestContext, TransitionPolicy::SingleTransition> fsm(TestState::Idle, &context);

    fsm.state(TestState::Idle)
        .onUpdate([](TestContext* ctx, double time) { return StateTransition::to(TestState::Running); });

    fsm.state(TestState::Running)
        .onEnter([](TestContext* ctx, double time) { ctx->enterCount++; });

    fsm.update(1.0);
    EXPECT_EQ(fsm.getCurrentState(), TestState::Running);
    EXPECT_EQ(context.enterCount, 0); // Not entered yet with SingleTransition

    fsm.update(2.0);
    EXPECT_EQ(context.enterCount, 1); // Now entered
}

