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

#include "serialization.h"

#include <deque>
#include <errno.h>
#include <mutex>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <vector>

// Globals

class ProfilerState;
static ProfilerState *g_profilerState;

// Definitions

class ProfilerState {
public:
	ProfilerState()
	        : m_taskScheduler(nullptr),
	          m_savedSuspendIds(),
	          m_output(nullptr), m_outputLock() {
	}

private:
	ftl::TaskScheduler *m_taskScheduler;

	std::vector<uint64_t> m_savedSuspendIds; // Per fiber

	FILE *m_output;
	std::mutex m_outputLock;

public:
	bool Init(ftl::TaskScheduler *taskScheduler, const char *outputFilePath);

	void RegisterFibers(unsigned fiberCount);

	uint64_t SpanStart(const char *category, const char *name);
	void SpanEnd(uint64_t spanId);

	void FiberSuspend(unsigned fiberIndex);
	void FiberResume(unsigned fiberIndex);

	void Close();
};

uint64_t hash(uint64_t x) {
	x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
	x = x ^ (x >> 31);
	return x;
}

inline uint64_t hash_combine(uint64_t seed) {
	return seed;
}

template <typename... Rest>
inline uint64_t hash_combine(uint64_t seed, const uint64_t &v, Rest... rest) {
	seed ^= hash(v) + UINT64_C(0x9e3779b9) + (seed << 6) + (seed >> 2);
	return hash_combine(seed, rest...);
}

bool InitProfiler(ftl::TaskScheduler *taskScheduler, const char *outputFilePath) {
	g_profilerState = new ProfilerState();
	return g_profilerState->Init(taskScheduler, outputFilePath);
}

void CloseProfileFile() {
	g_profilerState->Close();
}

void TermProfiler() {
	delete g_profilerState;
	g_profilerState = nullptr;
}

void RegisterFibers(unsigned fiberCount) {
	g_profilerState->RegisterFibers(fiberCount);
}

uint64_t SpanStart(const char *category, const char *name) {
	return g_profilerState->SpanStart(category, name);
}

void SpanEnd(uint64_t spanId) {
	g_profilerState->SpanEnd(spanId);
}

void FiberSuspend(unsigned fiberIndex) {
	g_profilerState->FiberSuspend(fiberIndex);
}

void FiberResume(unsigned fiberIndex) {
	g_profilerState->FiberResume(fiberIndex);
}

bool ProfilerState::Init(ftl::TaskScheduler *taskScheduler, const char *outputFilePath) {
	m_taskScheduler = taskScheduler;
	m_output = fopen(outputFilePath, "wb");
	if (m_output == nullptr) {
		printf("Failed to open output file: %d", errno);
		return false;
	}
	if (setvbuf(m_output, nullptr, _IOFBF, 64 * 1024) != 0) {
		printf("Failed to set the buffer size for output file: %d", errno);
		return false;
	}

	return true;
}

void ProfilerState::RegisterFibers(unsigned fiberCount) {
	m_savedSuspendIds.resize(fiberCount);
	for (unsigned i = 0; i < fiberCount; ++i) {
		m_savedSuspendIds[i] = std::numeric_limits<uint64_t>::max();
	}
}

uint64_t ProfilerState::SpanStart(const char *category, const char *name) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	uint64_t spanId = hash_combine(0, tid, now);

	std::lock_guard<std::mutex> guard(m_outputLock);
	WriteSpanStart(m_output, category, name, tid, spanId, now);

	return spanId;
}

void ProfilerState::SpanEnd(uint64_t spanId) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	std::lock_guard<std::mutex> guard(m_outputLock);
	WriteSpanEnd(m_output, tid, spanId, now);
}

void ProfilerState::FiberSuspend(unsigned fiberIndex) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	uint64_t suspendId = hash_combine(0, tid, now);
	m_savedSuspendIds[fiberIndex] = suspendId;

	std::lock_guard<std::mutex> guard(m_outputLock);
	WriteSpanSuspend(m_output, tid, suspendId, now);
}

void ProfilerState::FiberResume(unsigned fiberIndex) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	uint64_t suspendId = m_savedSuspendIds[fiberIndex];

	std::lock_guard<std::mutex> guard(m_outputLock);
	WriteSpanResume(m_output, tid, suspendId, now);
}

void ProfilerState::Close() {
	std::lock_guard<std::mutex> guard(m_outputLock);
	fflush(m_output);
	fclose(m_output);
}
