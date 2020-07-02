/* 
 * Copyright 2013-2015 Tom McDermott, N5EG
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
 *
 *  Adaptations for this application are Copyright 2020 m5evt
 *  and remain under GNU General Public License, as above.
 */
#ifndef HL2radio_hh
#define HL2radio_hh 
#include <iostream>
#include <string>
//#include "HL2udp.hh"
#include <semaphore.h>
#include "HL2ipaddr.hh"
#include <vector>

#define NUMRXIQBUFS	16		// number of receiver IQ buffers in circular queue.
					                // Must be integral power of 2 (2,4,8,16,32,64, etc.)

#define RXBUFSIZE	256		  // number of floats in one RxIQBuf, #complexes is half
                          // Must be integral power of 2 (2,4,8,16,32,64, etc.)

#define NUMTXBUFS	128		  // number of transmit buffers in circular queue
                          // Must be integral power of 2

#define TXBUFSIZE	512		  // number of bytes in one TxBuf

#define TXINITIALBURST	  4		// Number of Ethernet frames to holdoff before bursting
					                    // to fill hardware TXFIFO
using std::cout;
using std::endl;

class HL2udp;

class HL2radio {
 private:
    enum {	
      RXSTREAM_OFF,		// Receiver  RX IQ 
      RXSTREAM_ON,		// Stream Control
    };	
    
    enum {	
      PRIMARY = 0,		  // HL purpose, primary = tx,
      SECONDARY = 1,		// secondary = rx and tx packets must be send as 0
    };	    
    
    // Functions
    void StopStream(void);
    void InitialiseHL2(void);  
    void BuildControlRegs(unsigned reg_num, unsigned char* outbuf); 
    void PrintRawBuf(unsigned char* inbuf);
    float* GetNextRxBuf();
    void BuildI2Cwrite(unsigned int data, unsigned int addr, unsigned char* outbuf); 

    // Variables
    unsigned char* raw_packet;
    
    unsigned txfreq;
    unsigned rx0freq;
    unsigned rx1freq;
    unsigned rx2freq;
    unsigned rx3freq;
    unsigned rx4freq;

	  bool adc_overload;
	  bool Duplex;
    int lna_gain;               // HL2 LNA Gain
    bool OnboardPA;             // HL2 Onboard PA
    bool Q5Switch;              // HL2 Q5 switch external PTT in low power mode
    sem_t rx_sem;               // RX semaphore

	  unsigned char hl2_version;

    //****************************
	  float* RxIQBuf[NUMRXIQBUFS]; // ReceiveIQ buffers
	  unsigned RxWriteCounter;	  // Which Rx buffer to write to
	  unsigned RxReadCounter;		  // Which Rx buffer to read from
	  unsigned RxWriteFill;		  // Fill level of the RxWrite buffer

	  unsigned char* TxBuf[NUMTXBUFS]; 	// Transmit buffers

	  unsigned long LostRxBufCount;	// Lost-buffer counter for packets we actually got
	  unsigned long TotalRxBufCount;	// Total buffer count (may roll over)
	  unsigned long LostTxBufCount;	//
	  unsigned long TotalTxBufCount;	//
	  unsigned long CorruptRxCount;	//
	  unsigned long LostEthernetRx;	//
	  unsigned long CurrentEthSeqNum;	// Diagnostic
    
    bool reset_clock_sync;
    bool enable_sync_lync;
    bool reset_all_sync;
    bool reset_NCO_sync;
    int last_rxfreq;
    
	  bool TxStop;

    int hl2_type;
    
    HL2ipaddr* ipaddr;
    HL2udp* udp;    
    
 public:
    //Constructor:
      
    void CloseUdp(void); 
        
    HL2radio(int radio_num, HL2ipaddr* ipaddr) : ipaddr(ipaddr) { 
      hl2_type = radio_num;    
      reset_clock_sync = 0;
      enable_sync_lync = 0;
      reset_all_sync = 0;
      reset_NCO_sync = 0;
    };

    // Deconstructor
    ~HL2radio() {   
    	fprintf(stderr, "\nLostRxBufCount = %lu  TotalRxBufCount = %lu"
                      "  LostTxBufCount = %lu  TotalTxBufCount = %lu"
		                  "  CorruptRxCount = %lu  LostEthernetRx = %lu\n",
	                    LostRxBufCount, TotalRxBufCount, LostTxBufCount,
		                  TotalTxBufCount, CorruptRxCount, LostEthernetRx);
      CloseUdp();
    }
    // Functions
    int Discover(void);    
    int RxConnect(void);
    void ReceiveRxIQ(unsigned char * inbuf);    
    unsigned char* GetRxIQ();     
    void StartStream(void);
    void ForwardFromPC(unsigned char *buff);
    void StartReceiveIqThread(void);
    void ResetAllSync(void);    
    void ResetNCOSync(const int &rxfreq);
    // Variables
    int rx_sample_rate;
    int num_receivers;    
};

#endif // HL2radio_hh
