# ComputerNetworks
This is the repo for Computer Networks that I took in junior fall. 

## Project 1: TCP
This project aims to mimic the behavior of TCP behavior when we transmit a file. Reliable data transfer and congestion control are implemented.
We ran the project (written in C) in a **linux remote server**.

### Feature 1: Reliable Data Transfer
Each packet transferred is marked with a sequence number (sender) or acknowledge number (receiver). If packets arrive out of order, retransmission is triggered (Or one packet is acknowledged multiple times)
Sliding window is also implemented to allow pipelining, where multiple packets will be in flight.

### Feature 2: TCP Congestion Control
We set up a retransmission timer that's calculated based on Karn's algorithm. Exponential backoff is implemented as well.
Congestion control is achieved by three core functionalities: slow start, congestion avoidance and fast retransmit.
The congestion window is plotted with matplotlib

## Project 2: FTP server
A simplified version of FTP application protocol consisting of FTP client and FTP server. The FTP server is responsible for maintaining FTP sessions and providing file access. The FTP client is split into two components: an FTP user interface and an FTP client to make requests to the FTP server. The client provides a simple user interface with a command prompt asking the user to input a command that will be issued to the FTP server.

