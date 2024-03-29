#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"
#include "queue.h"


/*
 * You are required to change the implementation to support
 * window size greater than one.
 * In the current implementation the window size is one, hence we have
 * only one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    //An integer keeping track of next expected acknowledge number
    int next_exp_ackno = 0;
    struct Queue buffer_queue;
    initQueue(&buffer_queue);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        //buffer change to character
        recvpkt = (tcp_packet *) buffer;
        //printf("received seqno: %d\n", recvpkt->hdr.seqno);
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        if ( recvpkt->hdr.data_size == 0) {
            VLOG(INFO, "End Of File has been reached");
            fclose(fp);
            break;
        }
        //if squence number is not next_expected
        if(next_exp_ackno == 0){
	    	//Initialize
            next_exp_ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
        }else if(recvpkt->hdr.seqno > next_exp_ackno){
            //receive an out of order packet, store in buffer
          
		enqueue(&buffer_queue, recvpkt);
            
        }else if(recvpkt->hdr.seqno == next_exp_ackno){
            //Packet arrive in order
            next_exp_ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            //check if buffer is empty, if not, dequeue all, 
            while(!isEmpty(&buffer_queue) && peek(&buffer_queue)->hdr.seqno == next_exp_ackno){
                tcp_packet* dequeue_pkt = dequeue(&buffer_queue);
                //update next_exp_ackno, send cumulative ackno
                next_exp_ackno = dequeue_pkt->hdr.seqno + dequeue_pkt->hdr.data_size;  
            }
        }
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

    	fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
    	fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
        sndpkt = make_packet(0);
        sndpkt->hdr.ackno = next_exp_ackno;
        sndpkt->hdr.ctr_flags = ACK;
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }
    }

    return 0;
}




/*Sender:
- extend windoe size to 10
- keep count on last being ACKed
- retranmission timer
- Maintain Slide windoe forward, received, send*
- base point to oldest packet
- next seq now point to send
- buffer last packet added
- monitor for duplicate(3)
- */


/*Sender:
- receive 10
- kcommunative ack
- Out of order packer, buffer
- next expected receive
- */
