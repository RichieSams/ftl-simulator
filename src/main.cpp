/**
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2020
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "profiler.h"

#include "ftl/task_counter.h"
#include "ftl/task_scheduler.h"

#include <array>
#include <stdint.h>
#include <stdio.h>
#include <thread>

void Consumer(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	PROFILE_SPAN("Test");

	auto *globalCounter = reinterpret_cast<std::atomic<unsigned> *>(arg);

	globalCounter->fetch_add(1);
	//std::this_thread::sleep_for(std::chrono::microseconds(500));
}

void Producer(ftl::TaskScheduler *taskScheduler, void *arg) {
	PROFILE_SPAN("Test");
	constexpr unsigned kNumConsumerTasks = 50U;

	auto *tasks = new ftl::Task[kNumConsumerTasks];
	for (unsigned i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = {Consumer, arg};
	}

	{
		PROFILE_SPAN("Test", "Producer subsection 1");
		ftl::TaskCounter counter1(taskScheduler);
		taskScheduler->AddTasks(kNumConsumerTasks, tasks, ftl::TaskPriority::Low, &counter1);

		taskScheduler->WaitForCounter(&counter1);
	}

	{
		PROFILE_SPAN("Test", "Producer subsection 2");

		ftl::TaskCounter counter2(taskScheduler);
		taskScheduler->AddTasks(kNumConsumerTasks, tasks, ftl::TaskPriority::Low, &counter2);

		taskScheduler->WaitForCounter(&counter2);
	}

	// Cleanup
	delete[] tasks;
}

int main() {
	ftl::TaskScheduler taskScheduler;
	InitProfiler(&taskScheduler, "ftl-sim.fib");

	ftl::TaskSchedulerInitOptions options;
	options.Behavior = ftl::EmptyQueueBehavior::Yield;
	options.FiberPoolSize = 200;
	options.ThreadPoolSize = 12;
	options.Callbacks.OnFibersCreated = [](void *context, unsigned fiberCount) {
		RegisterFibers(fiberCount);
	};
	options.Callbacks.OnFiberStateChanged = [](void *context, unsigned fiberIndex, ftl::FiberState newState) {
		switch (newState) {
		case ftl::FiberState::Attached:
			FiberResume(fiberIndex);
			break;
		case ftl::FiberState::Detached:
			FiberSuspend(fiberIndex);
			break;
		}
	};

	if (taskScheduler.Init(options) < 0) {
		printf("TaskScheduler initialization failed");
		return 1;
	}

	constexpr unsigned kNumProducerTasks = 100U;
	std::array<ftl::Task, kNumProducerTasks> tasks;

	for (int i = 0; i < 2; ++i) {
		PROFILE_SPAN("", "Frame");

		std::atomic<unsigned> globalCounter(0U);
		FTL_VALGRIND_HG_DISABLE_CHECKING(&globalCounter, sizeof(globalCounter));

		for (auto &&task : tasks) {
			task = {Producer, &globalCounter};
		}

		ftl::TaskCounter counter(&taskScheduler);
		taskScheduler.AddTasks(kNumProducerTasks, tasks.data(), ftl::TaskPriority::Low, &counter);
		taskScheduler.WaitForCounter(&counter, true);

		printf("Counter value: %u\n", globalCounter.load());
	}

	CloseProfileFile();
	TermProfiler();
}
