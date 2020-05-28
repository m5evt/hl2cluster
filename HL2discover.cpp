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
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "HL2discover.hh"

#include <thread>

#define PORT 1024
#define DISCOVERY_SEND_PORT PORT
#define DISCOVERY_RECEIVE_PORT PORT
#define DATA_PORT PORT

#define inaddrr(x) (*(struct in_addr *) &ifr->x[sizeof sa.sin_port])

int HL2discover::get_addr(int sock, const char * ifname) {
  struct ifreq *ifr;
  struct ifreq ifrr;
  struct sockaddr_in sa;

  unsigned char      *u;
  int i;

  // new code to get all interface names on this host
  struct ifconf ifc;
  char buf[8192];
  struct ifreq *ifquery;
  int nInterfaces;
  
  // Query available interfaces. 
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if(ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
    printf("ioctl(SIOCGIFCONF) error");
    return -1;
  }

  // Iterate through the list of interfaces
  ifquery = ifc.ifc_req;
  nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

  for(int i=0; i<nInterfaces; i++) {
    fprintf(stderr, "Interface[%d]:%s  ", i, (char *)&ifquery[i].ifr_name);
  }
  fprintf(stderr,"\n");

  ifr = &ifrr;
  ifrr.ifr_addr.sa_family = AF_INET;
  strncpy(ifrr.ifr_name, ifname, sizeof(ifrr.ifr_name));

  if (ioctl(sock, SIOCGIFADDR, ifr) < 0) {
    printf("No %s interface.\n", ifname);
    return -1;
  }

  ip_address=inaddrr(ifr_addr.sa_data).s_addr;

  if (ioctl(sock, SIOCGIFHWADDR, ifr) < 0) {
    printf("No %s interface.\n", ifname);
    return -1;
  }

  u = (unsigned char *) &ifr->ifr_addr.sa_data;

  for(i=0;i<6;i++)
      hw_address[i]=u[i];

  return 0;
}

// Sends a Broadcast message and waits for a reply
void HL2discover::DiscoverAllHL2(const char* interface) {
    int rc;
    int i;

    fprintf(stderr,"Looking for Hermes-Lite2 on interface %s\n",interface);
    
    // send a broadcast to locate HL2 boards on the network
    discovery_socket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(discovery_socket<0) {
        perror("create socket failed for discovery_socket\n");
        exit(1);
    }

    // get my MAC address and IP address
    if(get_addr(discovery_socket,interface)<0) {
        exit(1);
    }

    printf("%s IP Address: %ld.%ld.%ld.%ld\n",
              interface,
              ip_address&0xFF,
              (ip_address>>8)&0xFF,
              (ip_address>>16)&0xFF,
              (ip_address>>24)&0xFF);

    printf("%s MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
         interface,
         hw_address[0], hw_address[1], hw_address[2], hw_address[3], hw_address[4], hw_address[5]);
    
    int optval = 1;
    setsockopt(discovery_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind to this interface
    interface_addr = {0};
    interface_addr.sin_family = AF_INET;
    interface_addr.sin_addr.s_addr = ip_address;
    interface_addr.sin_port = htons(0);
    if(bind(discovery_socket,(struct sockaddr*)&interface_addr,sizeof(interface_addr)) < 0) {
      perror("bind failed. Error");
    }

    printf("discover: bound to %s\n",interface);


    // allow broadcast on the socket
    int on = 1;
    rc=setsockopt(discovery_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    if(rc != 0) {
        fprintf(stderr,"cannot set SO_BROADCAST: rc=%d\n", rc);
        exit(1);
    }

    // setup to address
    struct sockaddr_in to_addr={0};
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(DISCOVERY_SEND_PORT);
    to_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // start a receive thread to get discovery responses
    discover_thread = std::thread(&HL2discover::HL2DiscoverThread, this); 
    // Keep the thread running after exit
    //rx_thread.detach();

    buffer[0]=0xEF;
    buffer[1]=0xFE;
    buffer[2]=0x02;
    for(i=0;i<60;i++) {
        buffer[i+3]=0x00;
    }
    if(sendto(discovery_socket,buffer,63,0,(struct sockaddr*)&to_addr,sizeof(to_addr))<0) {
        perror("sendto socket failed for discovery_socket\n");
        exit(1);
    }
    
    discover_thread.join();
    close(discovery_socket);
}

//Should this still take a JSradio object?
void HL2discover::HL2DiscoverThread(void) {
  struct sockaddr_in addr;
  socklen_t len;
  unsigned char buffer[2048];
  int bytes_read;
  struct timeval tv;
  int i;
  int version;

  char ip[16];
  char mac[18];  

  tv.tv_sec = 2;
  tv.tv_usec = 0;
  version=0;

  setsockopt(discovery_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

  len=sizeof(addr);
  
  while(1) {
    bytes_read=recvfrom(discovery_socket,buffer,sizeof(buffer),0,(struct sockaddr*)&addr,&len);
    if(bytes_read<0) {
      printf("discovery: bytes read %d\n", bytes_read);
      perror("discovery: recvfrom socket failed for discover_receive_thread");
      break;
    }
    
    if ((buffer[0] & 0xFF) == 0xEF && (buffer[1] & 0xFF) == 0xFE) {
      int status = buffer[2] & 0xFF;
      if (status == 2) {
        version=buffer[9]&0xFF; 
        if ((buffer[10] & 0xFF) == 0x06) {
          cout << "Found an HL2" << endl;
          cout << "Gateware " << (buffer[9] & 0xFF);
          cout << " p" << (buffer[0x15] & 0xFF) << endl;
          cout << endl;
  
          // get MAC address from reply
          sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                  buffer[3]&0xFF,buffer[4]&0xFF,buffer[5]&0xFF,
                  buffer[6]&0xFF,buffer[7]&0xFF,buffer[8]&0xFF);
          
          // get ip address from packet header
          sprintf(ip,"%d.%d.%d.%d",
                  addr.sin_addr.s_addr&0xFF,
                  (addr.sin_addr.s_addr>>8)&0xFF,
                  (addr.sin_addr.s_addr>>16)&0xFF,
                  (addr.sin_addr.s_addr>>24)&0xFF);
          
          std::string mac_address(mac);
          std::string ip_address(ip);          
          
          hl2discovered.emplace_back(mac_address, ip_address);      
        }
      }
    }
  }
  printf("discovery: exiting discover_receive_thread\n"); 
}

std::string HL2discover::ReadDiscoveredMAC(int i) {
 return hl2discovered[i].ReadMAC();
}

std::string HL2discover::ReadDiscoveredIP(int i) {
 return hl2discovered[i].ReadIP();
}

int HL2discover::ReadNumHL2Discovered(void) {
  return static_cast<int>(hl2discovered.size());
}
