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

#include <stdlib.h>

#include <signal.h>
#include <cstdlib>
#include <unistd.h>
#include <math.h>

#include "hl2cluster.hh"
#include "HL2discover.hh"
#include "HL2server.hh"

int main(int argc, char **argv) {
  // Collect command line arguments
	for (int i = 1; i < argc; i++) {
    cout << argv[i] << "\n";
  }

  // This interface should be called from the command line?
  const char interface[16] = "enp7s0";
  
  // Look for all Hermes Lite 2 SDRs on the interface
  HL2discover* network = 0;
  network = new HL2discover();  
  network->DiscoverAllHL2(interface);
  
  // Now check that the HL2s that we want to use have been discovered
  HL2server* cluster = 0;
  cluster = new HL2server(network);
  cluster->AddHL2s();
  
  // Start the main working thread
  // Opens a socket which listens for a discovery packet sent by PC SDR software
  cluster->StartClusterServer();    
    
  return 0;

}




