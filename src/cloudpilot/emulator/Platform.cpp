#include "Platform.h"

#include <chrono>
#include <cstring>
#include <ctime>

#if defined(ESP_PLATFORM)
    #include <sys/time.h>
#endif

long Platform::GetMilliseconds() {
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
}

void Platform::GetTime(uint32& hour, uint32& min, uint32& sec) {
    time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    tm t;
    localtime_r(&time, &t);

    hour = t.tm_hour;
    min = t.tm_min;
    sec = t.tm_sec;
}

void Platform::GetDate(uint32& year, uint32& month, uint32& day) {
    time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());

    tm t;
    localtime_r(&time, &t);

    year = t.tm_year + 1900;
    month = t.tm_mon + 1;
    day = t.tm_mday;
}

void Platform::SetPalmTime(uint32 secondsSince1904) {
#if defined(ESP_PLATFORM)
    // Palm OS counts local time from 1904-01-01; the ESP32 clock runs in UTC with no time zone,
    // so GetTime/GetDate (localtime_r) read back exactly what is set here.
    constexpr int64_t kPalmToUnixOffset = 2082844800;  // seconds from 1904-01-01 to 1970-01-01
    const timeval tv = {static_cast<time_t>(static_cast<int64_t>(secondsSince1904) - kPalmToUnixOffset), 0};
    settimeofday(&tv, nullptr);
#else
    (void)secondsSince1904;
#endif
}

void* Platform::AllocateMemory(size_t count) {
    void* mem = malloc(count);

    return mem;
}

void* Platform::AllocateMemoryClear(size_t count) {
    void* mem = Platform::AllocateMemory(count);
    memset(mem, 0, count);

    return mem;
}

uint32 Platform::Random() { return rand(); }
