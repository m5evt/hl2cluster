/* 
 * Copyright 2020 m5evt
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef HL2ipaddr_hh
#define HL2ipaddr_hh

#include <string>

class HL2ipaddr {
  private:
    std::string ip_address;    
    std::string mac_address;      
  
  public:
    HL2ipaddr (std::string mac, std::string ip) {   
      mac_address = mac;           
      ip_address = ip;
    };
  
    // Functions
    std::string ReadMAC(void);
    std::string ReadIP(void);
};

#endif
