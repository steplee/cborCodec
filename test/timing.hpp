
#pragma once

#include <sys/stat.h>
#include <time.h>


namespace {
	int64_t getMicros() {
			timespec start;
			clock_gettime(CLOCK_REALTIME, &start);
			return start.tv_sec * 1'000'000 + start.tv_nsec / 1'000;
	}
}
