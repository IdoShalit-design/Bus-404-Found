#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H
#include <WiFi.h>



class NetworkManager{
    
  private:
    String ssid;
    String password;

    public:
    NetworkManager(String ssid, String password);   
  /**
   * @brief connects to a given wifi network
   * 
   * @param ssid the "name" of the network
   * @param password the password fo the network
   * @return true if connected
   * @return false if couldn't connect
   */
    bool connect_to_wifi();

    /**
   * @brief scane for available networks and 
   *        prints their details description
   */
  void print_networks();

  /**
   * @brief prints wifi connection problems
   * 
   */
  void print_wifi_status();
    
};

#endif