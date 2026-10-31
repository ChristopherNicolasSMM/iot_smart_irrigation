#pragma once
// Copy to provision.h (gitignored). Never commit real secrets.
#ifndef IOT_WIFI_SSID
#define IOT_WIFI_SSID ""
#endif
#ifndef IOT_WIFI_PASSWORD
#define IOT_WIFI_PASSWORD ""
#endif
#ifndef IOT_ADMIN_PASSWORD
#define IOT_ADMIN_PASSWORD ""
#endif
#ifndef IOT_OPERATOR_PASSWORD
#define IOT_OPERATOR_PASSWORD ""
#endif
#ifndef IOT_PORTAL_URL
#define IOT_PORTAL_URL "" // HTTPS origin only; no trailing slash
#endif
#ifndef IOT_DEVICE_TOKEN
#define IOT_DEVICE_TOKEN ""
#endif
#ifndef IOT_PORTAL_CA
#define IOT_PORTAL_CA ""   // PEM CA, mandatory for HTTPS
#endif
#ifndef IOT_DEVICE_GUID
#define IOT_DEVICE_GUID "" // Existing portal GUID or empty for a NEW device
#endif
#ifndef IOT_PHYSICAL_OUTPUTS
#define IOT_PHYSICAL_OUTPUTS 0
#endif
#ifndef IOT_RELAY_ACTIVE_LOW
#define IOT_RELAY_ACTIVE_LOW 1
#endif
#ifndef IOT_RELAY_MASK
#define IOT_RELAY_MASK 0 // Bits for physically verified relay channels
#endif
// RS485 pins/classic ESP32: verify transceiver, level and external pull-down.
#ifndef IOT_RS485_ENABLED
#define IOT_RS485_ENABLED 0
#endif
#ifndef IOT_RS485_RX
#define IOT_RS485_RX 16
#endif
#ifndef IOT_RS485_TX
#define IOT_RS485_TX 17
#endif
#ifndef IOT_RS485_DE
#define IOT_RS485_DE 19
#endif
#ifndef IOT_NODE_ADDRESS
#define IOT_NODE_ADDRESS 1
#endif

#ifndef IOT_NODE_MAX_RUN_MS
#define IOT_NODE_MAX_RUN_MS 600000
#endif

// Stable physical bindings: empty means unbound, never guess by array order.
#ifndef IOT_ACTUATOR_ID_1
#define IOT_ACTUATOR_ID_1 ""
#endif
#ifndef IOT_ACTUATOR_ID_2
#define IOT_ACTUATOR_ID_2 ""
#endif
#ifndef IOT_ACTUATOR_ID_3
#define IOT_ACTUATOR_ID_3 ""
#endif
#ifndef IOT_ACTUATOR_ID_4
#define IOT_ACTUATOR_ID_4 ""
#endif
#ifndef IOT_MOISTURE_ID_1
#define IOT_MOISTURE_ID_1 ""
#endif
#ifndef IOT_MOISTURE_ID_2
#define IOT_MOISTURE_ID_2 ""
#endif
#ifndef IOT_MOISTURE_ID_3
#define IOT_MOISTURE_ID_3 ""
#endif
#ifndef IOT_MOISTURE_ID_4
#define IOT_MOISTURE_ID_4 ""
#endif
#ifndef IOT_TEMPERATURE_ID_1
#define IOT_TEMPERATURE_ID_1 ""
#endif
#ifndef IOT_TEMPERATURE_ID_2
#define IOT_TEMPERATURE_ID_2 ""
#endif
#ifndef IOT_TEMPERATURE_ID_3
#define IOT_TEMPERATURE_ID_3 ""
#endif
#ifndef IOT_TEMPERATURE_ID_4
#define IOT_TEMPERATURE_ID_4 ""
#endif
