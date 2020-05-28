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
#ifndef HL2cluster_hh
#define HL2cluster_hh

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <cerrno>
#include <cstring>

#include <vector>

#include "HL2discover.hh"
#include "HL2radio.hh"

class HL2server {
  private:
    int num_hl2s_incluster;
    std::vector<HL2ipaddr> cluster_ipaddrs;
    std::vector<HL2radio*> radios;
    
    std::thread pc_thread;
    std::thread hl2_thread;    
    
    int num_receivers;
        
    struct sockaddr_in servaddr, cliaddr;    
    int sockfd;
    
    HL2discover* discovered;
    // Functions
    int ReadHL2List(void);
    void WaitForDiscovery(void);
    void ClusterThread(void);
    void ClusterHL2toPCThread(void);    
    
  public:
    HL2server(HL2discover* discovered) : discovered(discovered) {
      num_hl2s_incluster = 0;
      memset(&servaddr, 0, sizeof(servaddr)); 
      memset(&cliaddr, 0, sizeof(cliaddr)); 
    };
    // Functions
    void AddHL2s(void);
    void StartIQStreams(void);
    void StartClusterServer(void);      
};

#endif
