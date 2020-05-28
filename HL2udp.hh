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

#ifndef HL2udp_hh
#define HL2udp_hh

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

#include "HL2radio.hh"

class HL2radio;

class HL2udp {
  private:
    enum {	
      RXSTREAM_OFF,		// Hermes Receiver Stream Controls
      RXSTREAM_ON,		// Narrow Band (down converted)
    };	
  
    std::thread rx_thread;
  
    int data_socket;
    
    struct sockaddr_in interface_addr;
    
    unsigned char hw_address[6];
    long ip_address;

    struct sockaddr_in data_addr;
    int data_addr_length;

    long send_sequence = -1;
    unsigned char buffer[70];

    int found=0;

    int ep;
    long sequence=-1;    
    
    unsigned char output_buffer[1032];
    int offset=8;    
    
    // Functions
    int get_addr(int sock, const char * ifname);

  public: 
    // Functions    
    void metis_stop_receive_thread(void);    
    
    void InitialiseConnection(std::string hl2_ip);    
    void StartRxThread(HL2radio *hl2);

    void StopStream(void);
    
    void metis_receive_stream_control(unsigned char);    
    int metis_write(unsigned char ep,unsigned char* buffer,int length); 
       
    void metis_receive_thread(HL2radio *hl2); 
    
    void metis_send_buffer(unsigned char* buffer,int length);
};

#endif  // HL2udp_hh


