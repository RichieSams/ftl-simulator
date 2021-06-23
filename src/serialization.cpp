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

#include "serialization.h"

#include <limits>
#include <string.h>

enum class EventType : uint8_t {
	SpanStart = 0,
	SpanEnd = 1,
	SpanSuspend = 2,
	SpanResume = 3,
};

inline void WriteByte(FILE *file, uint8_t value) {
	if (fwrite(&value, sizeof(uint8_t), 1, file) != 1) {
		printf("Failed to write uint8");
	}
}

inline void WriteUInt16(FILE *file, uint16_t value) {
	if (fwrite(&value, sizeof(uint16_t), 1, file) != 1) {
		printf("Failed to write uint16");
	}
}

inline void WriteUInt64(FILE *file, uint64_t value) {
	if (fwrite(&value, sizeof(uint64_t), 1, file) != 1) {
		printf("Failed to write uint16");
	}
}

inline void WriteString(FILE *file, const char *value) {
	size_t len = strlen(value);
	if (len > std::numeric_limits<uint16_t>::max()) {
		return;
	}

	fwrite(&len, sizeof(uint16_t), 1, file);
	fwrite(value, sizeof(char), len, file);
}

void WriteSpanStart(FILE *file, const char *category, const char *name, uint64_t tid, uint64_t spanId, uint64_t timeStamp) {
	WriteByte(file, (uint8_t)EventType::SpanStart);
	WriteString(file, category);
	WriteString(file, name);
	WriteUInt64(file, tid);
	WriteUInt64(file, spanId);
	WriteUInt64(file, timeStamp);
}

void WriteSpanEnd(FILE *file, uint64_t tid, uint64_t spanId, uint64_t timeStamp) {
	WriteByte(file, (uint8_t)EventType::SpanEnd);
	WriteUInt64(file, tid);
	WriteUInt64(file, spanId);
	WriteUInt64(file, timeStamp);
}

void WriteSpanSuspend(FILE *file, uint64_t tid, uint64_t suspendId, uint64_t timeStamp) {
	WriteByte(file, (uint8_t)EventType::SpanSuspend);
	WriteUInt64(file, tid);
	WriteUInt64(file, suspendId);
	WriteUInt64(file, timeStamp);
}

void WriteSpanResume(FILE *file, uint64_t tid, uint64_t suspendId, uint64_t timeStamp) {
	WriteByte(file, (uint8_t)EventType::SpanResume);
	WriteUInt64(file, tid);
	WriteUInt64(file, suspendId);
	WriteUInt64(file, timeStamp);
}
