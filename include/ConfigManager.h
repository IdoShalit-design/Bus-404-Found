/**
 * @file ConfigManager.h
 * @brief Declarations for application configuration management.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <cstddef>

/**
 * @brief Represents the current build mode of the firmware.
 */
enum class BuildState {
    Unknown,
    Production,
    Development
};

/**
 * @brief Aggregate holding configuration required to build the application.
 */
struct AppConfig {
    BuildState state;
    const char* lines[8];
    std::size_t lineCount;

    AppConfig() : state(BuildState::Unknown), lines{nullptr}, lineCount(0) {}
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
