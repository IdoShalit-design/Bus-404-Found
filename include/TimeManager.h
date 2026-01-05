#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h> // Standard C time library

class TimeManager {
private:
    const char* _timezone;
    const char* _ntpServer = "pool.ntp.org";

public:
    /**
     * @brief Construct a new Time Manager object
     * @param tz POSIX timezone string
     */
    TimeManager(const char* tz);

    /**
     * @brief Configures the NTP settings and starts the sync process
     */
    void init_and_sync();

    /**
     * @brief Checks if the internal clock has been synchronized with NTP
     * @return true if time is set
     */
    bool is_time_set();

    /**
     * @brief Gets the current time formatted as a string
     * @return String (HH:MM)
     */
    String get_formatted_time();

    /**
    * @brief Calculates minutes remaining until a given ETA
    * @param eta String in format "HH:MM"
    * @return int minutes remaining (negative if bus already passed)
    */
    int get_minutes_until(String eta);
};

#endif