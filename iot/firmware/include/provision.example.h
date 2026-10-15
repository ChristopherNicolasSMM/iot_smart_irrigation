#pragma once
// Copy to provision.h (gitignored). Never commit real secrets.
#define IOT_WIFI_SSID ""
#define IOT_WIFI_PASSWORD ""
#define IOT_ADMIN_PASSWORD ""
#define IOT_OPERATOR_PASSWORD ""
#define IOT_PORTAL_URL "" // HTTPS origin only; no trailing slash
#define IOT_DEVICE_TOKEN ""
#define IOT_PORTAL_CA ""   // PEM CA, mandatory for HTTPS
#define IOT_DEVICE_GUID "" // Existing portal GUID or empty for a NEW device
#define IOT_PHYSICAL_OUTPUTS 0
#define IOT_RELAY_ACTIVE_LOW 1
#define IOT_RELAY_MASK 0 // Bits for physically verified relay channels
// RS485 pins/classic ESP32: verify transceiver, level and external pull-down.
#define IOT_RS485_ENABLED 0
#define IOT_RS485_RX 16
#define IOT_RS485_TX 17
#define IOT_RS485_DE 19
#define IOT_NODE_ADDRESS 1

#define IOT_NODE_MAX_RUN_MS 600000

// Stable physical bindings: empty means unbound, never guess by array order.
#define IOT_ACTUATOR_ID_1 ""
#define IOT_ACTUATOR_ID_2 ""
#define IOT_ACTUATOR_ID_3 ""
#define IOT_ACTUATOR_ID_4 ""
#define IOT_MOISTURE_ID_1 ""
#define IOT_MOISTURE_ID_2 ""
#define IOT_MOISTURE_ID_3 ""
#define IOT_MOISTURE_ID_4 ""
#define IOT_TEMPERATURE_ID_1 ""
#define IOT_TEMPERATURE_ID_2 ""
#define IOT_TEMPERATURE_ID_3 ""
#define IOT_TEMPERATURE_ID_4 ""
