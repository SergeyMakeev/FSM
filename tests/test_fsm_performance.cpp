#include "fsm/fsm.h"
#include <chrono>
#include <gtest/gtest.h>

enum class PerfState
{
    State1,
    State2,
    State3,
    Count
};

struct PerfContext
{
    int counter = 0;
    int transitionThreshold = 1000;
};

// Helper to measure execution time
template <typename Func> double measureMs(Func&& func)
{
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// Test: Stateless lambdas should be fast (no std::function overhead)
TEST(FSMPerformanceTest, StatelessLambdasAreFast)
{
    PerfContext context;
    StateMachine<PerfState, PerfContext> fsm(PerfState::State1, &context);

    // Configure with stateless lambdas
    fsm.configureState(PerfState::State1)
        .onUpdate(
            [](PerfContext* ctx, double time)
            {
                ctx->counter++;
                if (ctx->counter >= ctx->transitionThreshold)
                    return StateTransition::switchTo(PerfState::State2);
                return StateTransition::stayInCurrent();
            });

    fsm.configureState(PerfState::State2)
        .onUpdate(
            [](PerfContext* ctx, double time)
            {
                ctx->counter++;
                if (ctx->counter >= ctx->transitionThreshold * 2)
                    return StateTransition::switchTo(PerfState::State3);
                return StateTransition::stayInCurrent();
            });

    fsm.configureState(PerfState::State3)
        .onUpdate(
            [](PerfContext* ctx, double time)
            {
                ctx->counter++;
                return StateTransition::stayInCurrent();
            });

    // Run many updates
    constexpr int updateCount = 10000;
    double elapsed = measureMs(
        [&]()
        {
            for (int i = 0; i < updateCount; ++i)
            {
                fsm.update(static_cast<double>(i));
            }
        });

    // Verify FSM executed correctly
    EXPECT_EQ(fsm.getCurrentState(), PerfState::State3);
    // Counter is updateCount + 2 due to chained transitions:
    // - Transition to State2 triggers State2.onUpdate (counter++)
    // - Transition to State3 triggers State3.onUpdate (counter++)
    EXPECT_EQ(context.counter, updateCount + 2);

    // Performance expectation: Should complete in reasonable time
    // On modern hardware, 10k updates should be well under 100ms
    std::cout << "Stateless lambdas: " << updateCount << " updates in " << elapsed << "ms (" << (elapsed / updateCount * 1000.0)
              << " μs/update)" << std::endl;

    EXPECT_LT(elapsed, 100.0) << "FSM is too slow for stateless lambdas";
}

// Test: Capturing lambdas also work (may have small overhead, but still fast)
TEST(FSMPerformanceTest, CapturingLambdasWork)
{
    PerfContext context;
    StateMachine<PerfState, PerfContext> fsm(PerfState::State1, &context);

    // External counter for demonstration
    int externalCounter = 0;

    // Configure with capturing lambdas
    fsm.configureState(PerfState::State1)
        .onUpdate(
            [&externalCounter](PerfContext* ctx, double time)
            {
                ctx->counter++;
                externalCounter++; // Capture external state
                if (ctx->counter >= ctx->transitionThreshold)
                    return StateTransition::switchTo(PerfState::State2);
                return StateTransition::stayInCurrent();
            });

    fsm.configureState(PerfState::State2)
        .onUpdate(
            [&externalCounter](PerfContext* ctx, double time)
            {
                ctx->counter++;
                externalCounter++;
                if (ctx->counter >= ctx->transitionThreshold * 2)
                    return StateTransition::switchTo(PerfState::State3);
                return StateTransition::stayInCurrent();
            });

    fsm.configureState(PerfState::State3)
        .onUpdate(
            [&externalCounter](PerfContext* ctx, double time)
            {
                ctx->counter++;
                externalCounter++;
                return StateTransition::stayInCurrent();
            });

    // Run many updates
    constexpr int updateCount = 10000;
    double elapsed = measureMs(
        [&]()
        {
            for (int i = 0; i < updateCount; ++i)
            {
                fsm.update(static_cast<double>(i));
            }
        });

    // Verify FSM executed correctly
    EXPECT_EQ(fsm.getCurrentState(), PerfState::State3);
    // Counter is updateCount + 2 due to chained transitions (see above test)
    EXPECT_EQ(context.counter, updateCount + 2);
    EXPECT_EQ(externalCounter, updateCount + 2);

    std::cout << "Capturing lambdas: " << updateCount << " updates in " << elapsed << "ms (" << (elapsed / updateCount * 1000.0)
              << " μs/update)" << std::endl;

    EXPECT_LT(elapsed, 100.0) << "FSM is too slow for capturing lambdas";
}

// Comparison test to show overhead is minimal
TEST(FSMPerformanceTest, StatelessVsCapturingComparison)
{
    constexpr int iterations = 5;
    constexpr int updatesPerIteration = 10000;

    double statelessTotal = 0.0;
    double capturingTotal = 0.0;

    for (int iter = 0; iter < iterations; ++iter)
    {
        // Test stateless
        {
            PerfContext context;
            StateMachine<PerfState, PerfContext> fsm(PerfState::State1, &context);

            fsm.configureState(PerfState::State1)
                .onUpdate(
                    [](PerfContext* ctx, double time)
                    {
                        ctx->counter++;
                        return StateTransition::stayInCurrent();
                    });

            statelessTotal += measureMs(
                [&]()
                {
                    for (int i = 0; i < updatesPerIteration; ++i)
                        fsm.update(static_cast<double>(i));
                });
        }

        // Test capturing
        {
            PerfContext context;
            StateMachine<PerfState, PerfContext> fsm(PerfState::State1, &context);

            int dummy = 0;
            fsm.configureState(PerfState::State1)
                .onUpdate(
                    [&dummy](PerfContext* ctx, double time)
                    {
                        ctx->counter++;
                        dummy++;
                        return StateTransition::stayInCurrent();
                    });

            capturingTotal += measureMs(
                [&]()
                {
                    for (int i = 0; i < updatesPerIteration; ++i)
                        fsm.update(static_cast<double>(i));
                });
        }
    }

    double statelessAvg = statelessTotal / iterations;
    double capturingAvg = capturingTotal / iterations;
    double overhead = ((capturingAvg - statelessAvg) / statelessAvg) * 100.0;

    std::cout << "\nPerformance Comparison (" << iterations << " iterations, " << updatesPerIteration << " updates each):" << std::endl;
    std::cout << "  Stateless:  " << statelessAvg << " ms/iteration" << std::endl;
    std::cout << "  Capturing:  " << capturingAvg << " ms/iteration" << std::endl;
    std::cout << "  Overhead:   " << overhead << "%" << std::endl;

    // The overhead should be minimal with modern std::function implementations
    // We accept up to 50% overhead as reasonable (actual is usually much less)
    EXPECT_LT(overhead, 50.0) << "Capturing lambda overhead is too high";
}
