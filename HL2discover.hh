/*
 *  Copyright (C)
 *  2009 - John Melton, G0ORX/N6LYT
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 *  Modifications only are Copyright 2012 - 2015, Tom McDermott, N5EG
 *  and remain under GNU General Public License, as above.
 * 
 *  Adaptations into c++ class are Copyright 2020 m5evt
 *  and remain under GNU General Public License, as above.
 */
#ifndef HL2discover_hh
#define HL2discover_hh

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <thread>
#include <vector>

#include <iostream>
using std::cout;
using std::endl;

#include "HL2ipaddr.hh"

class HL2discover {
  private:
    int discovery_socket;
    
    std::thread discover_thread;

    struct sockaddr_in discovery_addr;
    struct sockaddr_in interface_addr;
    
    int discovery_length;

    unsigned char hw_address[6];
    long ip_address;
    
    unsigned char buffer[70];
    // Hold all the discovered hl2 card in a vector
    std::vector<HL2ipaddr> hl2discovered;   
  
    // Functions
    int get_addr(int sock, const char * ifname);

  public:
    HL2discover () {};
    // Functions  
    void HL2DiscoverThread(void);      
    void DiscoverAllHL2(const char* interface); 
    std::string ReadDiscoveredMAC(int i);   
    std::string ReadDiscoveredIP(int i);  
    int ReadNumHL2Discovered(void);    
    
 
    
};

#endif
