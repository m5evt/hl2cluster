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
#include <string>
#include <thread>

#include "HL2udp.hh"

#define PORT 1024
#define DISCOVERY_SEND_PORT PORT
#define DISCOVERY_RECEIVE_PORT PORT
#define DATA_PORT PORT

#include <iostream>
using namespace std;
using std::cout;
using std::endl;

#define inaddrr(x) (*(struct in_addr *) &ifr->x[sizeof sa.sin_port])

int HL2udp::get_addr(int sock, const char * ifname) {
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

// close socket, stop receive thread, wait for thread to terminate
void HL2udp::metis_stop_receive_thread() {
  shutdown(data_socket, 2);
  
  // To kill a thread cleanly
  /*
  https://thispointer.com/c11-how-to-stop-or-terminate-a-thread/
  std::promise<void> exit_signal;
  std::future<void> future_obj = exit_signal.get_future();
  std::thread th(&func, std::move(future_obj));
  exit_signal.set_value();
  th.join();
  */
  
  rx_thread.join();
};

void HL2udp::InitialiseConnection(std::string hl2_ip) { 
  data_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(data_socket<0) {
    perror("protocol1: create socket failed for data_socket\n");
    exit(-1);
  }

  int optval = 1;
  if(setsockopt(data_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0) {
    perror("data_socket: SO_REUSEADDR");
  }
  
  int i;
  int rc;
  struct hostent *h;

  h=gethostbyname(hl2_ip.c_str());

  if(h==NULL) {
    //fprintf(stderr,"metis_start_receiver_stream unknown target.  MAC: %s    IP: %s\n",
    //metis_cards[0].mac_address, metis_cards[0].ip_address);
    exit(1);
  }
  
  data_addr_length = sizeof(data_addr);
  memset(&data_addr, 0, data_addr_length);
  data_addr.sin_family = AF_INET;
  data_addr.sin_port=htons(DATA_PORT);
  memcpy((char *)&data_addr.sin_addr.s_addr,h->h_addr_list[0],h->h_length);
  
  // bind to the interface
  if(bind(data_socket,(struct sockaddr*)&interface_addr,sizeof(interface_addr)) < 0) {
    perror("protocol1: bind socket failed for data_socket\n");
    exit(-1);    
  }
}

void HL2udp::StartRxThread(HL2radio *hl2) {
//void HL2udp::StartStream(void) {
  
  //memcpy(&data_addr,&radio->discovered->info.network.address,radio->discovered->info.network.address_length);
  //data_addr_length=radio->discovered->info.network.address_length;
  //data_addr.sin_port=htons(DATA_PORT);
  //break;
  
  // Start the receive thread
  cout << "Start rx thread" << endl;
  //rx_thread = std::thread(&HL2udp::metis_receive_thread, this); 
  rx_thread = std::thread(&HL2udp::metis_receive_thread, this, hl2); 
  // Keep the thread running after exit
  rx_thread.detach();
  
  //metis_receive_stream_control(RXSTREAM_ON, 0);  
}

void HL2udp::StopStream(void) {
  metis_receive_stream_control(RXSTREAM_OFF);
}

void HL2udp::metis_receive_stream_control(unsigned char streamControl) {
  // send a packet to start or stop the stream
  buffer[0]=0xEF;
  buffer[1]=0xFE;
  buffer[2]=0x04;    // data send state
  buffer[3]= streamControl;	// 0x0 = off, 0x01 = EP6 (NB data), 0x02 = (EP4) WB data, 0x03 = both on

  for (int i = 0; i < 60; i++) {
    buffer[i+4]=0x00;
  }
      
  metis_send_buffer(&buffer[0],1032);    
  /*
  if(sendto(discovery_socket,buffer,64,0,(struct sockaddr*)&data_addr,data_addr_length)<0) {
      perror("sendto socket failed for start\n");
      exit(1);
  }
  */
  if(streamControl == 0) {
    send_sequence = -1;	// reset HPSDR Tx Ethernet sequence number on stream stop
  }
}

void HL2udp::metis_receive_thread(HL2radio *hl2) {
  struct sockaddr_in addr;
  int length;
  unsigned char buffer[2048];
  int bytes_read;

  length=sizeof(addr);
    
  while(1) {
    bytes_read=recvfrom(data_socket, buffer, sizeof(buffer), 0, 
                        (struct sockaddr*)&addr, (socklen_t *)&length);

    if(bytes_read<0) {
      // new code to handle case of signal received            
      if (errno == EINTR) continue;
        perror("recvfrom socket failed for metis_receive_thread");
        exit(1);
      }

	  if(bytes_read == 0) continue;

	  if(bytes_read > 1048) fprintf(stderr, "Metis Receive Thread: bytes_read = %d  (>1048)\n", bytes_read);

    if(buffer[0]==0xEF && buffer[1]==0xFE) {
      switch(buffer[2]) {
        case 1:
          // get the end point
          ep=buffer[3]&0xFF;

          // get the sequence number
          sequence=((buffer[4]&0xFF)<<24)+((buffer[5]&0xFF)<<16)+((buffer[6]&0xFF)<<8)+(buffer[7]&0xFF);
          switch(ep) {
            case 6: // EP6			Send to Hermes Narrowband
              // process the data
              if(bytes_read != 1032) {
                fprintf(stderr,"Metis: bytes_read = %d (!= 1032)\n", bytes_read);
              }
          
              //if (radio != NULL) {
                hl2->ReceiveRxIQ(&buffer[0]); // send Ethernet frame to Proxy
              //}
              break;

            case 4: // EP4	 Wideband not implemented
              break;

            default:
              fprintf(stderr,"unexpected EP %d length=%d\n",ep,bytes_read);
              break;
          }

          break;

          default:
            fprintf(stderr,"unexpected packet type: 0x%02X\n",buffer[2]);
            break;
        }
    } else {
      fprintf(stderr,"received bad header bytes on data port %02X,%02X\n",buffer[0],buffer[1]);
    }
  }
}

int HL2udp::metis_write(unsigned char ep, unsigned char* buffer, int length) {
    int i;

    if(offset==8) {
        send_sequence++;
        output_buffer[0]=0xEF;
        output_buffer[1]=0xFE;
        output_buffer[2]=0x01;
        output_buffer[3]=ep;
        output_buffer[4]=(send_sequence>>24)&0xFF;
        output_buffer[5]=(send_sequence>>16)&0xFF;
        output_buffer[6]=(send_sequence>>8)&0xFF;
        output_buffer[7]=(send_sequence)&0xFF;
        
        // copy the buffer over
        for(i=0;i<512;i++) {
            output_buffer[i+offset]=buffer[i];
        }
        offset=520;
    } 
    else {
        // copy the buffer over
        for(i=0;i<512;i++) {
            output_buffer[i+offset]=buffer[i];
        }
        offset=8;

        // send the buffer
        metis_send_buffer(&output_buffer[0],1032);
    }

    return length;
}

void HL2udp::metis_send_buffer(unsigned char* buffer,int length) {
  if(sendto(data_socket,buffer,length,0,(struct sockaddr*)&data_addr,data_addr_length)!=length) {
    perror("sendto socket failed for metis_send_data\n");
    exit(1);
  }
}
