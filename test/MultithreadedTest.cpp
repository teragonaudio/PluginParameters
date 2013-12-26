/*
 * Copyright (c) 2013 Teragon Audio. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

// Must be included before PluginParameters.h
#include "TestRunner.h"
// Force multithreaded build
#define PLUGINPARAMETERS_MULTITHREADED 1
#include "PluginParameters.h"

// Simulate a realtime audio system by sleeping a bit after processing events.
// Here we assume 11ms sleep per block, which is approximately the amount of
// time needed to process 512 samples at 44100Hz sample rate.
// Several blocks may be processed before async changes are received, but here
// we only want to ensure that the event was routed from async->realtime.
#define SLEEP_TIME_PER_BLOCK_MS 11
#define TEST_NUM_BLOCKS_TO_PROCESS 10

namespace teragon {

////////////////////////////////////////////////////////////////////////////////
// Observers
////////////////////////////////////////////////////////////////////////////////

class TestCounterObserver : public ParameterObserver {
public:
    TestCounterObserver(bool isRealtime = true) : ParameterObserver(),
    realtime(isRealtime), count(0) {}

    virtual ~TestCounterObserver() {}

    bool isRealtimePriority() const {
        return realtime;
    }

    virtual void onParameterUpdated(const Parameter *parameter) {
        count++;
    }

    const bool realtime;
    int count;
};

class TestCacheValueObserver : public TestCounterObserver {
public:
    TestCacheValueObserver(bool isRealtime = true) : TestCounterObserver(isRealtime), value(0) {}

    void onParameterUpdated(const Parameter *parameter) {
        TestCounterObserver::onParameterUpdated(parameter);
        value = parameter->getValue();
    }

    ParameterValue value;
};

////////////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////////////

class _Tests {
public:
    static bool testCreateConcurrentParameterSet() {
        ConcurrentParameterSet s;
        ASSERT_SIZE_EQUALS((size_t)0, s.size());
        return true;
    }

    static bool testCreateManyConcurrentParameterSets() {
        // Attempt to expose bugs caused by fast-exiting threads
        printf("\nCreating sets");
        for(int i = 0; i < 20; i++) {
            printf(".");
            ConcurrentParameterSet *s = new ConcurrentParameterSet();
            ASSERT_SIZE_EQUALS((size_t)0, s->size());
            delete s;
        }
        return true;
    }

    static bool testThreadsafeSetParameterRealtime() {
        ConcurrentParameterSet s;
        Parameter *p = s.add(new BooleanParameter("test"));
        ASSERT_NOT_NULL(p);
        ASSERT_FALSE(p->getValue());
        s.set(p, true);
        s.processRealtimeEvents();
        ASSERT(p->getValue());
        return true;
    }

    static bool testThreadsafeSetParameterAsync() {
        ConcurrentParameterSet s;
        Parameter *p = s.add(new BooleanParameter("test"));
        ASSERT_NOT_NULL(p);
        ASSERT_FALSE(p->getValue());
        s.set(p, true);
        while(!p->getValue()) {
            s.processRealtimeEvents();
            usleep(1000 * SLEEP_TIME_PER_BLOCK_MS);
        }
        ASSERT(p->getValue());
        return true;
    }

    static bool testThreadsafeSetParameterBothThreadsFromAsync() {
        ConcurrentParameterSet s;
        TestCacheValueObserver realtimeObserver(true);
        TestCacheValueObserver asyncObserver(false);
        Parameter *p = s.add(new BooleanParameter("test"));
        ASSERT_NOT_NULL(p);
        p->addObserver(&realtimeObserver);
        p->addObserver(&asyncObserver);
        ASSERT_FALSE(p->getValue());
        s.set(p, true);
        for(int i = 0; i < TEST_NUM_BLOCKS_TO_PROCESS; i++) {
            s.processRealtimeEvents();
            usleep(1000 * SLEEP_TIME_PER_BLOCK_MS);
        }
        ASSERT(p->getValue());
        ASSERT_INT_EQUALS(1, realtimeObserver.count);
        ASSERT_INT_EQUALS(1, (int)realtimeObserver.value);
        ASSERT_INT_EQUALS(1, asyncObserver.count);
        ASSERT_INT_EQUALS(1, (int)asyncObserver.value);
        return true;
    }

    static bool testThreadsafeSetParameterBothThreadsFromRealtime() {
        ConcurrentParameterSet s;
        TestCounterObserver realtimeObserver(true);
        TestCounterObserver asyncObserver(false);
        Parameter *p = s.add(new BooleanParameter("test"));
        ASSERT_NOT_NULL(p);
        p->addObserver(&realtimeObserver);
        p->addObserver(&asyncObserver);
        ASSERT_FALSE(p->getValue());
        s.set(p, true);
        for(int i = 0; i < TEST_NUM_BLOCKS_TO_PROCESS; i++) {
            s.processRealtimeEvents();
            usleep(1000 * SLEEP_TIME_PER_BLOCK_MS);
        }
        ASSERT(p->getValue());
        ASSERT_INT_EQUALS(1, realtimeObserver.count);
        ASSERT_INT_EQUALS(1, asyncObserver.count);
        return true;
    }

    static bool testThreadsafeSetParameterWithSender() {
        ConcurrentParameterSet s;
        TestCounterObserver realtimeObserver(true);
        TestCounterObserver asyncObserver(false);
        Parameter *p = s.add(new BooleanParameter("test"));
        ASSERT_NOT_NULL(p);
        p->addObserver(&realtimeObserver);
        p->addObserver(&asyncObserver);
        ASSERT_FALSE(p->getValue());
        s.set(p, true, &asyncObserver);
        for(int i = 0; i < TEST_NUM_BLOCKS_TO_PROCESS; i++) {
            s.processRealtimeEvents();
            usleep(1000 * SLEEP_TIME_PER_BLOCK_MS);
        }
        ASSERT(p->getValue());
        ASSERT_INT_EQUALS(1, realtimeObserver.count);
        // The sender should NOT be called for its own events
        ASSERT_INT_EQUALS(0, asyncObserver.count);
        return true;
    }
};

} // namespace teragon

using namespace teragon;

////////////////////////////////////////////////////////////////////////////////
// Run test suite
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    gNumFailedTests = 0;
    const int numIterations = 20;

    // Run the tests several times, which increases the probability of exposing
    // race conditions or other multithreaded bugs. Note that even by doing this,
    // we cannot guarantee with 100% certainty that race conditions do not exist.
    // Gotta love concurrent programming. :)
    for(int i = 0; i < numIterations; i++) {
        printf("Running tests, iteration %d/%d:\n", i, numIterations);    
        ADD_TEST(_Tests::testCreateConcurrentParameterSet());
        ADD_TEST(_Tests::testCreateManyConcurrentParameterSets());
        ADD_TEST(_Tests::testThreadsafeSetParameterAsync());
        ADD_TEST(_Tests::testThreadsafeSetParameterRealtime());
        ADD_TEST(_Tests::testThreadsafeSetParameterBothThreadsFromAsync());
        ADD_TEST(_Tests::testThreadsafeSetParameterBothThreadsFromRealtime());
        ADD_TEST(_Tests::testThreadsafeSetParameterWithSender());
    }

    if(gNumFailedTests > 0) {
        printf("\nFAILED %d tests\n", gNumFailedTests);
    }
    else {
        printf("\nAll tests passed\n");
    }

    return gNumFailedTests;
}
