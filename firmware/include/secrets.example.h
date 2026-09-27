#pragma once

// Copy this file to secrets.h (which git ignores) and fill in your details.

#define LB_WIFI_SSID "your-wifi-name"
#define LB_WIFI_PASSWORD "your-wifi-password"

// The MQTT broker Home Assistant uses (e.g. the Mosquitto add-on). Use an IP
// address: .local names need mDNS lookups, which slow down every wake.
#define LB_MQTT_HOST "192.168.1.10"
#define LB_MQTT_PORT 1883
#define LB_MQTT_USER "lite-brite"
#define LB_MQTT_PASSWORD "change-me"

// Password for over-the-air updates (pio run -e xiao_esp32s3_ota -t upload).
#define LB_OTA_PASSWORD "change-me-too"

// Optional: a static IP skips DHCP, which makes every wake 0.5-2 s faster.
// Pick an address outside your router's DHCP range.
// #define LB_STATIC_IP "192.168.1.60"
// #define LB_GATEWAY "192.168.1.1"
// #define LB_SUBNET "255.255.255.0"
// #define LB_DNS "192.168.1.1"
