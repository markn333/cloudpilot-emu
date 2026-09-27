#include "Platform.h"

#include <chrono>
#include <cstring>
#include <ctime>

#if defined(ESP_PLATFORM)
    #include <sys/time.h>

    #include "esp_timer.h"
#endif

long Platform::GetMilliseconds() {
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
}

namespace {
#if defined(ESP_PLATFORM)
    int64_t cachedAtUs = -1;
    time_t cachedTime = -1;
    tm cachedTm;
#endif

    void GetLocalTime(tm& t) {
#if defined(ESP_PLATFORM)
        // The RTC registers are read thousands of times per second, and both gettimeofday and
        // localtime_r take locks / critical sections on the ESP32. So:
        // - read the wall clock at most every kRecheckUs (esp_timer is a plain counter read),
        // - convert with gmtime_r: the ESP32 clock runs without a time zone, so local time is UTC,
        // - cache the result. Only the emulator task calls this.
        // Platform::SetPalmTime() invalidates the cache.
        constexpr int64_t kRecheckUs = 5000;

        const int64_t nowUs = esp_timer_get_time();
        if (cachedAtUs < 0 || nowUs - cachedAtUs >= kRecheckUs) {
            const time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());
            if (time != cachedTime) {
                gmtime_r(&time, &cachedTm);
                cachedTime = time;
            }
            cachedAtUs = nowUs;
        }

        t = cachedTm;
#else
        time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());
        localtime_r(&time, &t);
#endif
    }
}  // namespace

void Platform::GetTime(uint32& hour, uint32& min, uint32& sec) {
    tm t;
    GetLocalTime(t);

    hour = t.tm_hour;
    min = t.tm_min;
    sec = t.tm_sec;
}

void Platform::GetDate(uint32& year, uint32& month, uint32& day) {
    tm t;
    GetLocalTime(t);

    year = t.tm_year + 1900;
    month = t.tm_mon + 1;
    day = t.tm_mday;
}

void Platform::SetPalmTime(uint32 secondsSince1904) {
#if defined(ESP_PLATFORM)
    // Palm OS counts local time from 1904-01-01; the ESP32 clock runs in UTC with no time zone,
    // so GetTime/GetDate (gmtime_r) read back exactly what is set here.
    constexpr int64_t kPalmToUnixOffset = 2082844800;  // seconds from 1904-01-01 to 1970-01-01
    const timeval tv = {static_cast<time_t>(static_cast<int64_t>(secondsSince1904) - kPalmToUnixOffset), 0};
    settimeofday(&tv, nullptr);
    cachedAtUs = -1;
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
