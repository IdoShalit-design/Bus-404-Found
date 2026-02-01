#ifndef CREDENTIALS_H
#define CREDENTIALS_H

 /* @brief Struct to hold WiFi credentials.
 * Using const char* for memory efficiency in embedded systems.
 */
struct WifiCredentialsData {
    const char* ssid;
    const char* password;
};

// Include the actual secret values
#include "Secrets.h"

#endif
