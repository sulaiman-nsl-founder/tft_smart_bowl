#include "services/TimeService.h"
#include "services/Logger.h"
#include <WiFi.h>

namespace Services {

TimeService& TimeService::getInstance() {
    static TimeService instance;
    return instance;
}

void TimeService::begin() {
    LOG_INFO("TIME", 500, "Configuring SNTP with timezone IST-5:30...");
    configTzTime("IST-5:30", "pool.ntp.org", "time.nist.gov");
}

bool TimeService::isTimeSet() const {
    time_t now;
    time(&now);
    // 1700000000 is approx Nov 2023. If time is before this, it's not synced.
    return now > 1700000000;
}

String TimeService::getFormattedDate() const {
    if (!isTimeSet()) {
        return "--/--";
    }
    
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buf[32];
    // e.g. "Jul 15"
    strftime(buf, sizeof(buf), "%b %d", &timeinfo);
    return String(buf);
}

String TimeService::getFormattedTime() const {
    if (!isTimeSet()) {
        return "--:--";
    }
    
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buf[32];
    // e.g. "11:59" (24-hour format so it's compact)
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    return String(buf);
}

} // namespace Services
