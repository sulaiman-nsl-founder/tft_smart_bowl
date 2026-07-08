#pragma once
#include <Arduino.h>

namespace Protocols {
namespace Messages {

String makeOnlineStatus();
String makeTelemetry(uint32_t seq, int battery, int weight);

} // namespace Messages
} // namespace Protocols
