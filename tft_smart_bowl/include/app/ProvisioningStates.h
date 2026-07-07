#ifndef APP_PROVISIONING_STATES_H
#define APP_PROVISIONING_STATES_H

#include <stdint.h>

namespace App {

enum class ProvisioningState : uint8_t {
    None = 0,
    Boot,
    DeviceInfo,
    QR,
    Advertising,
    PhoneConnected,
    EnterPin,
    PinOk,
    PinFail,
    Locked,
    Sending,
    WifiConnecting,
    WifiConnected,
    WifiFailed,
    AutoConnected
};

} // namespace App

#endif // APP_PROVISIONING_STATES_H
