#include "MathBuffer.h"

template<typename T, size_t S>
constexpr MathBuffer<T,S>::MathBuffer() :
		headIndex(0), count(0) {
  static_assert(std::is_arithmetic<T>::value, "T must be numeric");
}

template<typename T,size_t S>
bool MathBuffer<T, S>::push(T value) {
  headIndex += 1;
  if (headIndex >= S) {
    headIndex = 0;
  }
  if (count < S) {
    count += 1;
  }

  buffer[headIndex] = value;
  bufferTimestamp[headIndex] = millis();

  return count == S; // Return true if buffer is full
}

template<typename T,size_t S>
void MathBuffer<T, S>::executeOnSamplesSince(int64_t cutoffMs, std::function<void (T, int64_t)> iterator) {
  for (int i = 0; i < count; i++) {
    int index = (headIndex - i); // going backward to go from newest to oldest
    if (index < 0) { // wrap around
      index += S;
    }
    if (bufferTimestamp[index] < cutoffMs) {
      return;
    }
    iterator(buffer[index], bufferTimestamp[index]);
  }
}

template<typename T,size_t S>
size_t MathBuffer<T, S>::countSamplesSince(int64_t cutoffMs) {
  for (int i = 0; i < count; i++) {
    int index = (headIndex - i); // going backward to go from newest to oldest
    if (index < 0) { // wrap around
      index += S;
    }
    if (bufferTimestamp[index] < cutoffMs) {
      return i;
    }
  }

  return count;
}


template<typename T,size_t S>
T MathBuffer<T, S>::averageSince(int64_t cutoffMs) {
  size_t sampleCount = countSamplesSince(cutoffMs);

  T average = 0;
  executeOnSamplesSince(cutoffMs, [&average, &sampleCount](T value, int64_t ms) {
    average += value / sampleCount;
  });

  return average;
}

template<typename T,size_t S>
T MathBuffer<T, S>::maxSince(int64_t cutoffMs) {
  T max = 0;
  bool isFirst = true;

  executeOnSamplesSince(cutoffMs, [&max, &isFirst](T value, int64_t ms) {
    if (isFirst || value > max) {
      max = value;
      isFirst = false;
    }
  });

  return max;
}

template<typename T,size_t S>
T MathBuffer<T, S>::minSince(int64_t cutoffMs) {
  T min = 0;
  bool isFirst = true;

  executeOnSamplesSince(cutoffMs, [&min, &isFirst](T value, int64_t ms) {
    if (isFirst || value < min) {
      min = value;
      isFirst = false;
    }
  });

  return min;
}

template<typename T,size_t S>
T MathBuffer<T, S>::firstValueOlderThan(int64_t cutoffMs) {
  for (int i = 0; i < count; i++) {
    int index = (headIndex - i); // going backward to go from newest to oldest
    if (index < 0) { // wrap around
      index += S;
    }
    if (bufferTimestamp[index] < cutoffMs) {
      return buffer[index];
    }
  }
  return 0;
}

