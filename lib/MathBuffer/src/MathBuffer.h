#pragma once
#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>
#include <type_traits>

template<typename T, size_t S> class MathBuffer {
public:
	constexpr MathBuffer();

	static constexpr size_t capacity = S;

	bool push(T value);

	void executeOnSamplesSince(int64_t cutoffMs, std::function<void (T, int64_t)> iterator);
	size_t countSamplesSince(int64_t cutoffMs);
	T averageSince(int64_t cutoffMs);
	T maxSince(int64_t cutoffMs);
	T minSince(int64_t cutoffMs);
	T firstValueOlderThan(int64_t cutoffMs);

private:
	T buffer[S];
	int64_t bufferTimestamp[S];

	size_t headIndex;
  size_t count;
};

#include "MathBuffer.tpp"
