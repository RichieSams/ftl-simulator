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

#include "ftl/task_counter.h"
#include "ftl/task_scheduler.h"

#include "optick.h"

#include <array>
#include <stdint.h>
#include <stdio.h>
#include <thread>

class ProfilerEventHandler {
public:
	ProfilerEventHandler(ftl::TaskScheduler *taskScheduler)
	        : m_taskScheduler(taskScheduler) {
	}

private:
	ftl::TaskScheduler *m_taskScheduler;
	std::vector<Optick::EventStorage *> m_fiberEventStorages;

public:
	void OnFibersCreated(unsigned fiberCount) {
		m_fiberEventStorages.resize(fiberCount);
		for (unsigned i = 0; i < fiberCount; ++i) {
			Optick::RegisterFiber(i, &m_fiberEventStorages[i]);
		}
	}
	void OnFiberStateChange(unsigned fiberIndex, ftl::FiberState newState) {
		Optick::EventStorage **currentThreadStorageSlot = Optick::GetEventStorageSlotForCurrentThread();

		// Profile session is not active
		if (*currentThreadStorageSlot == nullptr) {
			return;
		}

		if (fiberIndex > m_fiberEventStorages.size()) {
			printf("fiberIndex was too large. Received %u, expected < %zd\n", fiberIndex, m_fiberEventStorages.size());
			return;
		}

		Optick::EventStorage *fiberStorage = m_fiberEventStorages[fiberIndex];
		switch (newState) {
		case ftl::FiberState::Attached:
			Optick::FiberSyncData::AttachToThread(fiberStorage, m_taskScheduler->GetCurrentThreadIndex());
			break;
		case ftl::FiberState::Detached:
			Optick::FiberSyncData::DetachFromThread(fiberStorage);
			break;
		}
	}
};

void Consumer(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	OPTICK_EVENT();
	auto *globalCounter = reinterpret_cast<std::atomic<unsigned> *>(arg);

	globalCounter->fetch_add(1);
	std::this_thread::sleep_for(std::chrono::microseconds(500));
}

void Producer(ftl::TaskScheduler *taskScheduler, void *arg) {
	OPTICK_EVENT();
	constexpr unsigned kNumConsumerTasks = 50U;

	auto *tasks = new ftl::Task[kNumConsumerTasks];
	for (unsigned i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = {Consumer, arg};
	}

	{
		OPTICK_EVENT("Producer subsection 1");
		ftl::TaskCounter counter1(taskScheduler);
		taskScheduler->AddTasks(kNumConsumerTasks, tasks, ftl::TaskPriority::Low, &counter1);

		taskScheduler->WaitForCounter(&counter1);
	}

	{
		OPTICK_EVENT("Producer subsection 2");

		ftl::TaskCounter counter2(taskScheduler);
		taskScheduler->AddTasks(kNumConsumerTasks, tasks, ftl::TaskPriority::Low, &counter2);

		taskScheduler->WaitForCounter(&counter2);
	}

	// Cleanup
	delete[] tasks;
}

int main() {
	Optick::StartCapture();

	ftl::TaskScheduler taskScheduler;

	ProfilerEventHandler eventHandler(&taskScheduler);

	ftl::TaskSchedulerInitOptions options;
	options.Behavior = ftl::EmptyQueueBehavior::Sleep;
	options.FiberPoolSize = 200;
	options.ThreadPoolSize = 12;
	options.Callbacks.Context = &eventHandler;
	options.Callbacks.OnFibersCreated = [](void *context, unsigned fiberCount) {
		ProfilerEventHandler *handler = reinterpret_cast<ProfilerEventHandler *>(context);
		handler->OnFibersCreated(fiberCount);
	};
	options.Callbacks.OnWorkerThreadStarted = [](void *context, unsigned threadIndex) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "ftl-worker %u", threadIndex);

		Optick::RegisterThread(buffer);
	};
	options.Callbacks.OnWorkerThreadEnded = [](void *context, unsigned threadIndex) {
		Optick::UnRegisterThread(false);
	};
	options.Callbacks.OnFiberStateChanged = [](void *context, unsigned fiberIndex, ftl::FiberState newState) {
		ProfilerEventHandler *handler = reinterpret_cast<ProfilerEventHandler *>(context);
		handler->OnFiberStateChange(fiberIndex, newState);
	};

	if (taskScheduler.Init(options) < 0) {
		printf("TaskScheduler initialization failed");
		return 1;
	}

	constexpr unsigned kNumProducerTasks = 100U;
	std::array<ftl::Task, kNumProducerTasks> tasks;

	for (int i = 0; i < 2; ++i) {
		OPTICK_FRAME("MainThread");

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

	Optick::StopCapture();
	Optick::SaveCapture("ftl-simulator.opt");
}
