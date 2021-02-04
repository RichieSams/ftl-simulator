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

#include <thread>

static ProfilerState *g_profilerState;

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

void InitProfiler(ftl::TaskScheduler *taskScheduler) {
	g_profilerState = new ProfilerState(taskScheduler);
}

std::string DumpProfiler() {
	return g_profilerState->Dump();
}

void TermProfiler() {
	delete g_profilerState;
	g_profilerState = nullptr;
}

void RegisterThreads(unsigned threadCount) {
	g_profilerState->RegisterThreads(threadCount);
}

void RegisterFibers(unsigned fiberCount) {
	g_profilerState->RegisterFibers(fiberCount);
}

void SpanStart(const char *category, const char *name) {
	g_profilerState->SpanStart(category, name);
}

void SpanEnd() {
	g_profilerState->SpanEnd();
}

void FiberSuspend(unsigned fiberIndex) {
	g_profilerState->FiberSuspend(fiberIndex);
}

void FiberResume(unsigned fiberIndex) {
	g_profilerState->FiberResume(fiberIndex);
}

void ProfilerState::RegisterThreads(unsigned threadCount) {
	m_spanStack.resize(threadCount);
}

void ProfilerState::RegisterFibers(unsigned fiberCount) {
	m_savedSpanStacks.resize(fiberCount);
}

void ProfilerState::SpanStart(const char *category, const char *name) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	uint64_t spanId = hash_combine(0, tid, now);
	m_spanStack[tid].push_back(spanId);

	std::lock_guard<std::mutex> guard(m_bufferLock);
	m_buffer << "SpanStart " << category << " " << name << " threadId: " << tid << " spanId: " << spanId << " now: " << now << "\n";
}

void ProfilerState::SpanEnd() {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();
	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	uint64_t spanId = m_spanStack[tid].back();
	m_spanStack[tid].pop_back();

	std::lock_guard<std::mutex> guard(m_bufferLock);
	m_buffer << "SpanEnd threadId: " << tid << " spanId: " << spanId << " now: " << now << "\n";
}

void ProfilerState::FiberSuspend(unsigned fiberIndex) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();

	// Save the stack for resuming later
	m_savedSpanStacks[fiberIndex] = m_spanStack[tid];

	// Issue SpanSuspend events for the stack
	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	std::lock_guard<std::mutex> guard(m_bufferLock);
	for (auto iter = m_spanStack[tid].rbegin(); iter != m_spanStack[tid].rend(); ++iter) {
		m_buffer << "SpanSuspend threadId: " << tid << " spanId: " << *iter << " now: " << now << "\n";
	}
	m_spanStack[tid].clear();
}

void ProfilerState::FiberResume(unsigned fiberIndex) {
	uint64_t tid = m_taskScheduler->GetCurrentThreadIndex();

	m_spanStack[tid] = m_savedSpanStacks[fiberIndex];
	m_savedSpanStacks[fiberIndex].clear();

	// Issue SpanResume events for the stack
	uint64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	std::lock_guard<std::mutex> guard(m_bufferLock);
	for (auto iter = m_spanStack[tid].begin(); iter != m_spanStack[tid].end(); ++iter) {
		m_buffer << "SpanResume threadId: " << tid << " spanId: " << *iter << " now: " << now << "\n";
	}
}

std::string ProfilerState::Dump() {
	std::lock_guard<std::mutex> guard(m_bufferLock);

	return m_buffer.str();
}
