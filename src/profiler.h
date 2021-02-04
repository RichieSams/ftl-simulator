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

#pragma once

#include "ftl/task_scheduler.h"

#include <deque>
#include <mutex>
#include <sstream>
#include <stdint.h>
#include <vector>

enum class EventType {
	SpanStart,
	SpanEnd,
	FiberSuspend,
	FiberResume,
};

class ProfilerState {
public:
	ProfilerState(ftl::TaskScheduler *taskScheduler)
	        : m_taskScheduler(taskScheduler),
	          m_spanStack(),
	          m_savedSpanStacks(),
	          m_buffer(), m_bufferLock() {
	}

private:
	ftl::TaskScheduler *m_taskScheduler;

	std::vector<std::deque<uint64_t>> m_spanStack;       // per thread
	std::vector<std::deque<uint64_t>> m_savedSpanStacks; // per fiber

	std::stringstream m_buffer;
	std::mutex m_bufferLock;

public:
	void RegisterThreads(unsigned threadCount);
	void RegisterFibers(unsigned fiberCount);

	void SpanStart(const char *category, const char *name);
	void SpanEnd();

	void FiberSuspend(unsigned fiberIndex);
	void FiberResume(unsigned fiberIndex);

	std::string Dump();
};

void InitProfiler(ftl::TaskScheduler *taskScheduler);
std::string DumpProfiler();
void TermProfiler();

void RegisterThreads(unsigned threadCount);
void RegisterFibers(unsigned fiberCount);

void SpanStart(const char *category, const char *name);
void SpanEnd();
void FiberSuspend(unsigned fiberIndex);
void FiberResume(unsigned fiberIndex);

class ProfileSpan {
public:
	ProfileSpan(const char *category, const char *name) {
		SpanStart(category, name);
	}
	~ProfileSpan() {
		SpanEnd();
	}
};

// General utility macro
#define PP_CAT(A, B) A##B
#define PP_EXPAND(...) __VA_ARGS__

// Macro overloading feature support
#define PP_VA_ARG_SIZE(...) PP_EXPAND(PP_APPLY_ARG_N((PP_ZERO_ARGS_DETECT(__VA_ARGS__), PP_RSEQ_N)))

#define PP_ZERO_ARGS_DETECT(...) PP_EXPAND(PP_ZERO_ARGS_DETECT_PREFIX_##__VA_ARGS__##_ZERO_ARGS_DETECT_SUFFIX)
#define PP_ZERO_ARGS_DETECT_PREFIX__ZERO_ARGS_DETECT_SUFFIX , , , , , , , , , , , 0

#define PP_APPLY_ARG_N(ARGS) PP_EXPAND(PP_ARG_N ARGS)
#define PP_ARG_N(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define PP_RSEQ_N 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#define PP_OVERLOAD_SELECT(NAME, NUM) PP_CAT(NAME##_, NUM)
#define PP_MACRO_OVERLOAD(NAME, ...)                      \
	PP_OVERLOAD_SELECT(NAME, PP_VA_ARG_SIZE(__VA_ARGS__)) \
	(__VA_ARGS__)

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define UNIQUE_SPAN_NAME() CONCAT(span_, __COUNTER__)

#define PROFILE_SPAN(...) PP_MACRO_OVERLOAD(PROFILE_SPAN, __VA_ARGS__)

#define PROFILE_SPAN_0() \
	ProfileSpan UNIQUE_SPAN_NAME()("", __func__)
#define PROFILE_SPAN_1(category) \
	ProfileSpan UNIQUE_SPAN_NAME()(category, __func__)

#define PROFILE_SPAN_2(category, name) \
	ProfileSpan UNIQUE_SPAN_NAME()(category, name)
