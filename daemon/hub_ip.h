#pragma once

#include <cstdlib>

// Hub command/status TCP target.
// Unset or empty → 127.0.0.1 (DummyHub on this machine).
// systemd: EnvironmentFile=-/etc/grams/hub.conf  (HUB_IP=...)
// Lab Hub:  HUB_IP=192.168.1.100
// Flight:   HUB_IP=192.168.100.100
inline const char* HubIp() {
    const char* ip = std::getenv("HUB_IP");
    if (ip != nullptr && ip[0] != '\0') {
        return ip;
    }
    return "127.0.0.1";
}
