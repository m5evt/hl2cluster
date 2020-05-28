# hl2cluster
Hermes-Lite 2 cluster server

This allows 2 or more Hermes-Lite SDRs to be connected into a cluster and a single RX IQ packet is sent onwards to the PC. This is to open experiments with diveristy/coherent receivers.

This is experimental software and could break many things. Use at your own risk.

For now, the forwarding of RX IQ packets is limited to:

RX0 = HL2 1
RX1 = HL2 2

This will be changed in future to allow selection of HL2 based on RX ADC selection bits in protocol 1.

TX has not been taken care of properly and TX from a PC will result in both HL2s transmitting. I recommend disabling the PA in software for any experiments for now.

The interface to discover HL2s on is hard coded for now to enp7s0. To change, it is in hl2cluster.cpp, main():

  const char interface[16] = "enp7s0";
  
To select which HL2s to use in the cluster, edit hl2_list. The primary radio will be the first in the list.
