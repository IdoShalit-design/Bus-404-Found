#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stddef.h>  // size_t
#include <time.h>

/**
 * @brief Configures NTP and waits for time sync.
 * @param tz POSIX timezone string (e.g. "IST-2IDT,M3.4.4/26,M10.5.0")
 */
void time_init_and_sync(const char* tz);

/**
 * @brief Checks if the internal clock has been synchronized with NTP.
 * @return true if time is set
 */
bool time_is_set();

/**
 * @brief Gets the current time formatted as "HH:MM" into a caller-supplied buffer.
 * @param buf  Buffer of at least 6 bytes ("HH:MM\0")
 * @param len  Size of the buffer
 */
void time_get_formatted(char* buf, size_t len);

/**
 * @brief Calculates minutes remaining until a given ETA.
 * @param eta C-string in format "HH:MM"
 * @return int minutes remaining (negative if already passed)
 */
int time_get_minutes_until(const char* eta);

#endif