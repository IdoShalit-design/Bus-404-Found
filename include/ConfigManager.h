/**
 * @file ConfigManager.h
 * @brief Declarations for application configuration management.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <cstddef>

/**
 * @brief Represents the current build mode of the firmware.
 *
 * CURRENT: existing Hub75 + Curlbus flow.
 * STATION: future station-focused mode.
 * NY_METRO: future New York Metro mode.
 */
enum class BuildState {
    Current,
    Station,
    NyMetro
};

/**
 * @brief Aggregate holding configuration required to build the application.
 */
struct AppConfig {
    BuildState state;
    const char* lines[8];
    std::size_t lineCount;

    AppConfig() : state(BuildState::Current), lines{nullptr}, lineCount(0) {}
};

/**
 * @brief Handles loading and saving configuration for the application.
 */
class ConfigManager {
public:
    bool loadConfig(AppConfig& config);
    bool saveConfig(const AppConfig& config);
};

#endif // CONFIG_MANAGER_H
