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
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include "HL2server.hh"

// TODO: Move this to unsigned int in class
#define PORT 1024

using namespace std;

// Read the text file that has the configuration for this cluster.
// First MAC address is always the primary
// Check that all cards listed in the file have been discovered
int HL2server::ReadHL2List(void) {
  std::ifstream readfile;
  
  std::string hl2_list = "hl2_list";
  
  readfile.open(hl2_list);
  int rv = readfile.is_open();
  
  // Cannot open the file
  if (rv != 1) {
    cout << "Cannot open the hl2 list" << endl;
    return -1;
  }

  std::string line;

  int num_discovered_hl2s = discovered->ReadNumHL2Discovered();
  
  cout << num_discovered_hl2s << " hl2 cards discovered" << endl;
  
  if (num_discovered_hl2s < 2) return -1;
  
  // Crudely, read the line string.    
  while (getline(readfile, line)) {
    for (int i = 0; i < num_discovered_hl2s; i++) {
      if (line == discovered->ReadDiscoveredMAC(i)) {
        cluster_ipaddrs.emplace_back(line, discovered->ReadDiscoveredIP(i));
      }
    }
  }
  
  num_hl2s_incluster = static_cast<int>(cluster_ipaddrs.size());
  
  cout << "Using cards: " << endl;
  for (int i = 0; i < num_hl2s_incluster; i++) {
    cout << cluster_ipaddrs[i].ReadMAC() << endl;
  }
  
  return 0;
}

// Read the text file list and then add the radios to a vector
// of these objects.
// Connect to the radios, connection initialises the HL2 and 
// sends some config packets to the HL2
void HL2server::AddHL2s(void) {
  // Read text file containing the HL2s 
  // that we want to use
  int rv = ReadHL2List(); 
  if (rv < 0) exit(0);
  
  // Add the radios to the cluster
  cout << "Add the radios" << endl;
 
  for (int i = 0; i < num_hl2s_incluster; i++) {
    HL2ipaddr* ipaddr = 0;
    ipaddr = new HL2ipaddr(cluster_ipaddrs[i]);
    radios.emplace_back(new HL2radio(i, ipaddr));  
    radios[i]->RxConnect();
    cout << "Added HL2 " << i << endl;
  }
}

// Each udp connection to an HL2 runs a thread for the packets coming 
// from the HL2. This starts these threads.
void HL2server::StartIQStreams(void) {
  for (int i = 0; i < num_hl2s_incluster; i++) {  
    radios[i]->StartReceiveIqThread();
  }
}

// Start Servers/threads for connection from PC to this
// and this to n HL2s
void HL2server::StartClusterServer(void) {
  // start a receive thread to get discovery responses
  cout << "Start the listener" << endl;  
  
  WaitForDiscovery();
  
  pc_thread = std::thread(&HL2server::ClusterThread, this); 
  hl2_thread = std::thread(&HL2server::ClusterHL2toPCThread, this);   
  
  hl2_thread.join();
  pc_thread.join();   
}

// Sit waiting for a discovery packet
// This code could do with being more robust before deciding a discovery packet has come
// and we can leave this function
void HL2server::WaitForDiscovery(void) {
  cout << "Started cluster thread" << endl;
  
  // Creating socket file descriptor 
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("socket creation failed"); 
    exit(EXIT_FAILURE); 
  } 
  
  // Filling server information 
  servaddr.sin_family    = AF_INET; // IPv4 
  servaddr.sin_addr.s_addr = INADDR_ANY; 
  servaddr.sin_port = htons(PORT); 
      
  // Bind the socket with the server address 
  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) { 
    perror("bind failed"); 
    exit(EXIT_FAILURE); 
  } 
      
  int n; 
  socklen_t len;
  len = sizeof(cliaddr); 
  
  unsigned char buffer[1032];

  int bytes_read = recvfrom(sockfd, buffer, 1032, 0, ( struct sockaddr *) &cliaddr, &len);   

  if (bytes_read != 63) {
    cout << "It isn't a discovery packet" << endl;    
  }
    
  // TODO; move this to a better place
  unsigned char reply[20] = {0xef, 0xfe, 2, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 73, 0x6,
                             0, 0, 0, 0, 0, 0, 0, 0, 4};
   
  // This could/should all be done with better code 
  memset(buffer, 0, 60);
  
  for (int i = 0; i < 20; i++) {
    buffer[i] = reply[i];   
  }  
  
  for (int i = 0; i < 60; i++) {
    printf("%.2X", buffer[i]);    
  }
  
  sendto(sockfd, buffer, 60, 0, (struct sockaddr *)&cliaddr, len); 

  cout << "Discovery response sent" << endl;  
}

// PC to this thread. Intercepts packets from the PC.
void HL2server::ClusterThread(void) {
  unsigned char buffer[1032];

  // These should be in a parent class
  socklen_t len;
  len = sizeof(cliaddr);
    
  // Start the HL2udp rx threads
  StartIQStreams();

  int ep;

  while(1) {
    int bytes_read = recvfrom(sockfd, buffer, 1032, 0, ( struct sockaddr *) &cliaddr, &len);     
    
    if (bytes_read <= 0) continue;

    if(buffer[0]==0xEF && buffer[1]==0xFE) {
      // Find out how many receivers are used. Each HL2 is setup to have the same number
      if (buffer[11]==0x00 & 1) {
        num_receivers = ((buffer[15] >> 3) & 7) + 1;
      } 
      else if (buffer[11+512]==0x00 & 1) {
        // Might be in the second CC message in the packet
        num_receivers = ((buffer[15+512] >> 3) & 7) + 1;
      }
      
      switch(buffer[2]) {
        case 1:
          // get the end point
          ep=buffer[3]&0xFF;

          switch(ep) {
            case 6: 
              // EP6
              break;
            
            case 4: 
              // EP4
              break;
            
            case 2:   
              //cout << "EP2 msg received " << bytes_read << endl;
              //for (int i = 0; i < 10; i++) {
              //  fprintf("%.2X", buffer[i]);     
              //}
              //fprintf("\n");              
              // TODO, strip radio > 0 if MOX bit (or CWX bit) is set. Otherwise, all the HL2s start txing.
              radios[0]->ForwardFromPC(&buffer[0]); 
              radios[1]->ForwardFromPC(&buffer[0]);                              
              break;
            
            default:
              fprintf(stderr,"unexpected EP %d length=%d\n",ep,bytes_read);
              break;
          }
          break;
              
        case 2:  
          // response to a discovery packet
          fprintf(stderr,"unexepected discovery response when not in discovery mode\n");
          break;
            
        case 4:
          // Start/stop command
          // TODO, add a StopStream
          cout << "Stop/start command" << endl;
          for (int i = 0; i < num_hl2s_incluster; i++) {
            radios[i]->StartStream();
          }
          break;
              
        default:
          fprintf(stderr,"unexpected packet type: 0x%02X\n",buffer[2]);
          break;
        }
    } 
    else {
      fprintf(stderr,"received bad header bytes on data port %02X,%02X\n",buffer[0],buffer[1]);
    }
  };   
}

// HL2 to this thread
void HL2server::ClusterHL2toPCThread(void) {
  cout << "Started ClusterHL2toPCThread thread" << endl;

  socklen_t len;
  len = sizeof(cliaddr); 
  
  unsigned char buffer[1032];  

  // Vector to hold the packets from the HL2s
  std::vector<unsigned char*> packets_from_hl2s; 
  // Init them before thread
  for (int i = 0; i < num_hl2s_incluster; i++) {
    packets_from_hl2s.emplace_back();
  }

  
  while(1) {    

    // GetRxIQ has a semaphore that waits until data has arrived
    // from an HL2
    for (int i = 0; i < num_hl2s_incluster; i++) {    
      packets_from_hl2s[i] = radios[i]->GetRxIQ();
    }
    cout << endl;
    /*
    cout << "Send packet to PC" << endl;
    
    char ip[16];
    sprintf(ip,"%d.%d.%d.%d",
            cliaddr.sin_addr.s_addr&0xFF,
            (cliaddr.sin_addr.s_addr>>8)&0xFF,
            (cliaddr.sin_addr.s_addr>>16)&0xFF,
            (cliaddr.sin_addr.s_addr>>24)&0xFF);    
    printf("Send to IP address %s\n", ip);    
      
      
    for (int i = 0; i < 30; i++) {
      printf("%.2X", packet_from_hl2[i]);    
    }          
    cout << endl << endl;
    */
    
    // If 1 rx, just sent the packet straight on
    if (num_receivers == 1) {
      sendto(sockfd, packets_from_hl2s[0], 1032, 0, (struct sockaddr *)&cliaddr, len);       
    }  
    else {
      // Do the slicing/swaping of RXs 
      int rx = 0;
      int j = 0;
      // First half of the packet, format:
      // RX0, RX1, RX2 M1M0 repeat
      for (int i = 16; i < 520; i += 6) {
        for (j = 0; j < 6; j++) {
          packets_from_hl2s[0][i+j] = packets_from_hl2s[rx][i+j];
        }        
        rx++;
        if (rx == num_receivers) {
          // M1M0
          rx = 0; 
          i = i + j - 4;
        }
      }  
      // Skip over sync and second CC, and repeat method above      
      for (int i = 528; i < 1032; i += 6) {      
        for (int j = 0; j < 6; j++) {
          packets_from_hl2s[0][i+j] = packets_from_hl2s[rx][i+j];
        }        
        rx++;
        if (rx == num_receivers) {
          rx = 0;        
          i = i + j - 4;
        } 
      }       
      
      // Send onwards to PC
      sendto(sockfd, packets_from_hl2s[0], 1032, 0, (struct sockaddr *)&cliaddr, len);        
    }
  };   
}





