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
#include <algorithm>
#include <chrono>
#include <thread>

#include "HL2radio.hh"
#include "HL2udp.hh"

using namespace std;

int HL2radio::RxConnect(void) {
  
  int RxFreq0 = 7030000;
  int RxFreq1 = 14030000;
  int RxFreq2 = 14030000;    
  int RxFreq3 = 14030000;     
  int RxFreq4 = 14030000;     
  int txfreq = 14030000;
  
  int RxSmp = 48000;
  int NumRx = 1;
  
  bool AGC = 0;
  int LNAG = 18;
  bool PA = 0;
  bool Q5 = 0; 
    
	rx_sample_rate = RxSmp;
  num_receivers = 1;
  
  rx0freq = (unsigned)RxFreq0;
  rx1freq = (unsigned)RxFreq1;
  rx2freq = (unsigned)RxFreq2;
  rx3freq = (unsigned)RxFreq3;
  
	Duplex = true;		// Allows TxF to program separately from RxF
  
	// HL2 parameters
	lna_gain = LNAG;
	OnboardPA = PA;
	Q5Switch = Q5;
  
  TxStop = false;
  
	// RX semaphore
	sem_init(&rx_sem, 0, 0);

	// allocate the receiver buffers
	for (int i=0; i<NUMRXIQBUFS; i++) {
		RxIQBuf[i] = new float[RXBUFSIZE];
  }
	// allocate the transmit buffers
	for (int i=0; i<NUMTXBUFS; i++) {
		TxBuf[i] = new unsigned char[TXBUFSIZE];  
  }

  InitialiseHL2();
}

int HL2radio::Discover(void) {
  const char interface[16] = "enp7s0";
	//udp.metis_discover((const char *)(interface), this);
}  

void HL2radio::StartReceiveIqThread(void) {
  cout << "Start stream" << endl;
  udp->StartRxThread(this);
	//udp.metis_receive_stream_control(RXSTREAM_ON, 0, this);
}

void HL2radio::StartStream(void) {
  cout << "Start stream2" << endl;
  udp->metis_receive_stream_control(RXSTREAM_ON);
	//udp.metis_receive_stream_control(RXSTREAM_ON, 0, this);
}

void HL2radio::StopStream(void) {
	unsigned int metis_entry = 0;	// Index into Metis_card MAC table  
  fprintf(stderr, "Stop stream...\n");
  udp->StopStream();
	//udp.metis_receive_stream_control(RXSTREAM_OFF, metis_entry);	// stop Hermes Rx data stream  
}
  
void HL2radio::InitialiseHL2(void) {
  
  // Repurposed to send the initial registers to Hermes before starting the stream.
	// Ought to rename this as InitializeHermes or something similar.  
  udp = new HL2udp();
  udp->InitialiseConnection(ipaddr->ReadIP());

	// DEBUG
	fprintf(stderr, "InitialiseHL2...\n");

	unsigned char buffer[512];	// dummy up a USB HPSDR buffer;
	for(int i=0; i<512; i++) {
		buffer[i] = 0;
  }
	int length = 512;		// udp.metis_write ignores this value
	unsigned char ep = 0x02;	// all Hermes data is sent to end point 2

	// udp.metis_write needs to be called twice to make one ethernet write to the hardware
	// Set these registers before starting the receive stream

	BuildControlRegs(0, buffer);
	udp->metis_write(ep, buffer, length);
	BuildControlRegs(2, buffer);
	udp->metis_write(ep, buffer, length);

	BuildControlRegs(0, buffer);
	udp->metis_write(ep, buffer, length);
	BuildControlRegs(4, buffer);
	udp->metis_write(ep, buffer, length);

	BuildControlRegs(0, buffer);
	udp->metis_write(ep, buffer, length);
	BuildControlRegs(6, buffer);
	udp->metis_write(ep, buffer, length);

  // See https://github.com/softerhardware/Hermes-Lite2/wiki/External-Clocks

  if (hl2_type == PRIMARY) {
    
    unsigned char buffer2[512];	// dummy up a USB HPSDR buffer;
  	for(int i=0; i<512; i++) {
  		buffer[i] = 0;
    }
	  int length = 512;		// udp.metis_write ignores this value
    unsigned char ep = 0x02;	// all Hermes data is sent to end point 2
    
    // EnableCL2_sync76p8MHz
    cout << "i2c write" << endl;
    BuildI2Cwrite(0x62, 0x3b, buffer); // Clock2 CMOS1 output, 3.3V
	  udp->metis_write(ep, buffer, length);  
	  BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length);        
    std::this_thread::sleep_for(std::chrono::milliseconds(200));    
    
    BuildI2Cwrite(0x3d, 0x01, buffer); // OD1_intdiv[11:4]
	  udp->metis_write(ep, buffer, length); 
    BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length); 
    std::this_thread::sleep_for(std::chrono::milliseconds(200));     
    
    BuildI2Cwrite(0x3e, 0x10, buffer); // OD1_intdiv[3:0]
	  udp->metis_write(ep, buffer, length); 
    BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length); 
    std::this_thread::sleep_for(std::chrono::milliseconds(200));        
    
    BuildI2Cwrite(0x31, 0x81, buffer); // Enable divider for clock 2
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));      
    
    BuildI2Cwrite(0x3c, 0x00, buffer); // integer skew if required
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));        
    
    BuildI2Cwrite(0x3f, 0x1f, buffer); // fractional skew
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));      
    
    BuildI2Cwrite(0x63, 0x01, buffer); // Enable clock 2 output
	  udp->metis_write(ep, buffer, length);      
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
    cout << "done i2c write" << endl;    
    
    // Align clock 2 with clock 1
	  BuildControlRegs(0x72, buffer);     
	  udp->metis_write(ep, buffer, length);      
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    
  }
  else {
    // Secondary HL2
    unsigned char buffer2[512];	// dummy up a USB HPSDR buffer;
  	for(int i=0; i<512; i++) {
  		buffer[i] = 0;
    }
	  int length = 512;		// udp.metis_write ignores this value
    unsigned char ep = 0x02;	// all Hermes data is sent to end point 2
    
    // EnableCL2_sync76p8MHz
    cout << "i2c write" << endl;
    BuildI2Cwrite(0x17, 0x02, buffer); // FB_intdiv[11:4] Adjust multiplication for new 76.8MHz reference
	  udp->metis_write(ep, buffer, length);  
	  BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length);        
    std::this_thread::sleep_for(std::chrono::milliseconds(200));    
    
    BuildI2Cwrite(0x18, 0x20, buffer); // FB_intdiv[3:0]
	  udp->metis_write(ep, buffer, length); 
    BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length); 
    std::this_thread::sleep_for(std::chrono::milliseconds(200));     
    
    BuildI2Cwrite(0x10, 0xc0, buffer); // Enable both local oscillator and external clock inputs
	  udp->metis_write(ep, buffer, length); 
    BuildControlRegs(0xFF, buffer); 
	  udp->metis_write(ep, buffer, length); 
    std::this_thread::sleep_for(std::chrono::milliseconds(200));        
       
    BuildI2Cwrite(0x13, 0x03, buffer); // Switch to external clock
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));      
    
    BuildI2Cwrite(0x10, 0x44, buffer); // Enable external clock input only plus refmode
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));        
    
    BuildI2Cwrite(0x21, 0x0c, buffer); // Use previous channel, bypass divider
	  udp->metis_write(ep, buffer, length);     
	  BuildControlRegs(0xFF, buffer);     
	  udp->metis_write(ep, buffer, length);     
    std::this_thread::sleep_for(std::chrono::milliseconds(200));      
    
    cout << "done i2c write" << endl;     
  }

	// Initialize the first TxBuffer (currently empty) with a valid control frame (on startup only)
	BuildControlRegs(0, buffer);
	unsigned char* initial = TxBuf[0];
	for(int i=0; i<512; i++) {
		initial[i] = buffer[i];
  }
  
	return;  
}

void HL2radio::BuildI2Cwrite(unsigned int data, unsigned int addr, unsigned char* outbuf) {
	outbuf[0] = outbuf[1] = outbuf[2] = 0x7f;	// HPSDR USB sync
  
  outbuf[3] = 0x78; // i2c addr
  outbuf[4] = 0x06; // write cookie
  outbuf[5] = 0xea; // chip address
  outbuf[6] = data;  
  outbuf[7] = addr;   
}

void HL2radio::BuildControlRegs(unsigned reg_num, unsigned char* outbuf) {
	// create the sync + control register values to send to HL2
	// base on reg_num and the various parameter values.
	// reg_num must be even.

	unsigned char speed = 0;	// Rx sample rate
	unsigned char rx_ctrl = 0;	// Rx controls
	unsigned char ctrl_4 = 0;	// Rx register C4 control

	outbuf[0] = outbuf[1] = outbuf[2] = 0x7f;	// HPSDR USB sync

	outbuf[3] = reg_num;		// C0 Control Register (Bank Sel + PTT)
	//if (PTTMode == PTTOn)
	//  outbuf[3] |= 0x01;				// set MOX bit

	switch(reg_num)
	{
	  case 0:
	        speed = 0x0;
	        if(rx_sample_rate == 384000) speed |= 0x03;
	        if(rx_sample_rate == 192000) speed |= 0x02;
	        if(rx_sample_rate == 96000)  speed |= 0x01;
	        if(rx_sample_rate == 48000)  speed |= 0x00;

	        rx_ctrl = 0x00;
	        
	        ctrl_4 = 0x0;
	        ctrl_4 |= ((num_receivers-1) << 3) & 0x38;	// Number of receivers
          // V1.58 of protocol_1 spec allows
					// setting up to 8 receivers, but
          // I can't find which register to set the
          // Rx Frequency of the 8th receiver.
          // Hermes sends corrupted USB frames if
          // NumReceivers > 4.   Theoretically
          // Red Pitaya is OK to 6 (to be tested by
          // someone else).
	        if(Duplex) ctrl_4 |= 0x04;

	        outbuf[4] = speed;		// C1
	        outbuf[5] = 0x00;			// C2 // TODO HL2 open collector outputs
	        outbuf[6] = rx_ctrl;	// C3
	        outbuf[7] = ctrl_4;		// C4 - #Rx, Duplex
          break;

	  case 2:					// Tx NCO freq (and Rx1 NCO for special case)
	        outbuf[4] = ((unsigned char)(txfreq >> 24)) & 0xff;	// c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(txfreq >> 16)) & 0xff;	// c2
	        outbuf[6] = ((unsigned char)(txfreq >> 8)) & 0xff;	// c3
	        outbuf[7] = ((unsigned char)(txfreq)) & 0xff;		// c4 RxFreq LSB
          break;

	  case 4:					// Rx1 NCO freq (out port 0)
	        outbuf[4] = ((unsigned char)(rx0freq >> 24)) & 0xff;	// c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx0freq >> 16)) & 0xff;	// c2
	        outbuf[6] = ((unsigned char)(rx0freq >> 8)) & 0xff;	  // c3
	        outbuf[7] = ((unsigned char)(rx0freq)) & 0xff;	      // c4 RxFreq LSB
	        break;

	  case 6:					// Rx2 NCO freq (out port 1)
	        outbuf[4] = ((unsigned char)(rx1freq >> 24)) & 0xff;  // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx1freq >> 16)) & 0xff;  // c2
	        outbuf[6] = ((unsigned char)(rx1freq >> 8)) & 0xff;	  // c3
	        outbuf[7] = ((unsigned char)(rx1freq)) & 0xff;	      // c4 RxFreq LSB
	        break;

	  case 8:					// Rx3 NCO freq (out port 2)
	        outbuf[4] = ((unsigned char)(rx2freq >> 24)) & 0xff;  // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx2freq >> 16)) & 0xff;  // c2
	        outbuf[6] = ((unsigned char)(rx2freq >> 8)) & 0xff;	  // c3
	        outbuf[7] = ((unsigned char)(rx2freq)) & 0xff;	      // c4 RxFreq LSB
      	  break;

	  case 10:					// Rx4 NCO freq (out port 3)
    	    outbuf[4] = ((unsigned char)(rx3freq >> 24)) & 0xff; // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx3freq >> 16)) & 0xff; // c2
	        outbuf[6] = ((unsigned char)(rx3freq >> 8)) & 0xff;	 // c3
	        outbuf[7] = ((unsigned char)(rx3freq)) & 0xff;	 // c4 RxFreq LSB
	        break;
    
	  case 12:					// Rx5 NCO freq (out port 4)
	        outbuf[4] = ((unsigned char)(rx4freq >> 24)) & 0xff; // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx4freq >> 16)) & 0xff; // c2
	        outbuf[6] = ((unsigned char)(rx4freq >> 8)) & 0xff;	 // c3
	        outbuf[7] = ((unsigned char)(rx4freq)) & 0xff;	 // c4 RxFreq LSB
	        break;

	  case 14:					// Rx6 NCO freq (out port 5)
	        outbuf[4] = ((unsigned char)(rx4freq >> 24)) & 0xff; // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx4freq >> 16)) & 0xff; // c2
	        outbuf[6] = ((unsigned char)(rx4freq >> 8)) & 0xff;	 // c3
          outbuf[7] = ((unsigned char)(rx4freq)) & 0xff;	 // c4 RxFreq LSB
	        break;

	  case 16:					// Rx7 NCO freq (out port 6)
	        outbuf[4] = ((unsigned char)(rx4freq >> 24)) & 0xff; // c1 RxFreq MSB
	        outbuf[5] = ((unsigned char)(rx4freq >> 16)) & 0xff; // c2
	        outbuf[6] = ((unsigned char)(rx4freq >> 8)) & 0xff;	 // c3
	        outbuf[7] = ((unsigned char)(rx4freq)) & 0xff;	 // c4 RxFreq LSB
	        break;
    
	//	Note:  While Ver 1.58 of the HPSDR USB protocol doucment specifies up to 8 receivers,
	//	It only defines 7 receive frequency control register addresses. So we are currently
	//	limited to 7 receivers implemented.

	  case 18:					// drive level & HL2 PA
	        //if (PTTOffMutesTx & (PTTMode == PTTOff)) {
		        outbuf[4] = 0;				// (almost) kill Tx when PTTOff and PTTControlsTx
          //}
	        //else {
		        //outbuf[4] = TxDrive;			// c1
          //}
          // TODO HL2 VNA mode
	        outbuf[5] = 0;
          //if (OnboardPA) outbuf[5] |= 0x08;
	        //if (Q5Switch)  outbuf[5] |= 0x04;

	        // TODO HL2 VNA mode
	        outbuf[6] = 0;
	        outbuf[7] = 0;
	        break;

	  case 20:
	        // TODO Puresignal
	        outbuf[4] = 0;
	        outbuf[5] = 0;
	        outbuf[6] = 0;
	        outbuf[7] = ((lna_gain + 12) & 0x3f) | 0x40; // new gain protocol for HL2
	        break;
	
	  case 0xFF:
          outbuf[3] = 0;
	        outbuf[4] = 0;
	        outbuf[5] = 0;
	        outbuf[6] = 0;
	        outbuf[7] = 0;
	        break;	
    // 0x39          
	  case 114:
	        outbuf[4] = 0;
	        outbuf[5] = 0;
	        outbuf[6] = 0;
	        outbuf[7] = 1;
	        break;	          				

	  default:
	        fprintf(stderr, "Invalid Hermes/Metis register selection: %d\n", reg_num);
	        break;
	};

};

// for debugging
void HL2radio::PrintRawBuf(unsigned char* inbuf)	{
  fprintf(stderr, "Addr: %p    Dump of Raw Buffer\n", inbuf);
  for (int row=0; row<4; row++) {
      int addr = row * 16;
      fprintf(stderr, "%04X:  ", addr);
      for (int column=0; column<8; column++) {
	      fprintf(stderr, "%02X:", inbuf[row*16+column]);
      }
      fprintf(stderr, "...");
      for (int column=8; column<16; column++) {
	      fprintf(stderr, "%02X:", inbuf[row*16+column]);
      }
      fprintf(stderr, "\n");
    }
  fprintf(stderr, "\n");
};

// called by HL2udp Rx thread.
void HL2radio::ReceiveRxIQ(unsigned char * inbuf) {
	// look for lost receive packets based on skips in the HPSDR ethernet header
	// sequence number.
	unsigned int SequenceNum = (unsigned char)(inbuf[4]) << 24;
	SequenceNum += (unsigned char)(inbuf[5]) << 16;
	SequenceNum += (unsigned char)(inbuf[6]) << 8;
	SequenceNum += (unsigned char)(inbuf[7]);
  
  if(SequenceNum > CurrentEthSeqNum + 1) {
    LostEthernetRx += (SequenceNum - CurrentEthSeqNum);
	  CurrentEthSeqNum = SequenceNum;
	}
	else {
	  if(SequenceNum == CurrentEthSeqNum + 1) CurrentEthSeqNum++;
	}
	
  raw_packet = inbuf;
  sem_post(&rx_sem);
  return;
};

// next Readable Rx buffer
unsigned char* HL2radio::GetRxIQ() {
  sem_wait(&rx_sem);
  // get the next receiver buffer
	unsigned char* ReturnBuffer = raw_packet;	
  cout << " " << CurrentEthSeqNum << " ";
  // next readable Rx buffer
	return ReturnBuffer;		
};

void HL2radio::ForwardFromPC(unsigned char *buff) {
  //cout << "Message received from PC" << endl;
  /*
  for (int i = 0; i < 1032; i++) {
    fprintf("%.2X", buff[i]);     
  }
  fprintf("\n");  
  */
  udp->metis_send_buffer(&buff[0], 1032);
}

void HL2radio::CloseUdp(void) {
  udp->metis_receive_stream_control(RXSTREAM_OFF);	// stop Hermes data stream
  udp->metis_stop_receive_thread();	// stop receive_thread & close socket  
}
