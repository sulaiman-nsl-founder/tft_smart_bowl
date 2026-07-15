#ifndef SERVICES_TIME_SERVICE_H
#define SERVICES_TIME_SERVICE_H

#include <Arduino.h>
#include <time.h>

namespace Services {

class TimeService {
public:
    static TimeService& getInstance();

    void begin();
    
    bool isTimeSet() const;
    
    // Returns formatted date: e.g. "Jul 15"
    String getFormattedDate() const;
    
    // Returns formatted time: e.g. "11:59"
    String getFormattedTime() const;

private:
    TimeService() = default;
};

} // namespace Services

#endif // SERVICES_TIME_SERVICE_H
