#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <math.h>

#include "packet.h"
#include "common.h"
#include "queue.h"

#define STDIN_FD 0
#define a 0.125
#define b 0.25
#define upper_RTO 240000

double RTO = 3000; // millisecond - timeout duration
int backoff = 1;
int resend_flg = 0; //discard sample_rtt when retransmission

int nextToAck_seqno = 0; // temporary variable to control the while loop
int send_base = 0;  // seq no of base packet
int nextToSend_seqno = 0; // current packet send seqno
int EOF_flag = 0;
int window_size = 1; //initialization for slow start
int ssthresh = 64; //threshold for congestion control 
float calculated_cwnd = 0; //temporary variable for storing cwnd calculated in congestion avoidance

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; // struct for timer
struct timeval ack_time; // for calculating RTT
struct timeval time_init; //for intial time of prgram
struct timeval send_time; //temp timestamps storing when packets are sent
double sample_rtt = 0;
double estimated_rtt = 0;
double dev_rtt = 0;

tcp_packet *sndpkt;     // pointer to sending packet
tcp_packet *recvpkt;    // pointer for receaiving packet
sigset_t sigmask;       // signal, check signal sigalrm to resend packet

tcp_packet *basepkt;     //oldest not acknowledged packet
tcp_packet *headpkt;  //temp variable for freeing dequeued packet

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return fabs((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
}

//helper function for outputing CSV, every time window size changes
void writeToCSV(float time, float window, int threshold) { 
    FILE* file = fopen("cwnd.csv", "a"); // Open the file in "append" mode to add to an existing file or create a new one

    if (file == NULL) {
        perror("Failed to open CSV file");
        return;
    }

    fprintf(file, "%f,%f,%d\n", time, window, threshold);

    fclose(file);
}

void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        // Resend oldest not acknowledge packet, store in basepkt global variable
        //VLOG(INFO, "Timout happend");

        printf("Timeout for %d happened!\n",basepkt->hdr.seqno);

        //mulplicative decrease when packet loss happens
        ssthresh = fmax(2, window_size / 2);
        window_size = 1;
        struct timeval t1; //dummy var for counting
        gettimeofday(&t1,0);
        //printf("Window size: %d\n", window_size);
        writeToCSV(timedifference_msec(time_init, t1), window_size, ssthresh);


        resend_flg = 1; //set resend_flg to 1
        backoff *= 2; //double backoff when retransmission
        if (sendto(sockfd, basepkt, TCP_HDR_SIZE + get_data_size(basepkt), 0,
                   (const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
        //Adjust RTO and set new timer
        RTO = backoff * RTO; //actual RTO after backoff
        if (RTO > upper_RTO){ //check if RTO reaches upper bound
            RTO = upper_RTO;
        }
        //printf("Current RTO after resend: %lf\n", RTO);
        timer.it_value.tv_sec = (int)RTO / 1000; // sets an initial value
        timer.it_value.tv_usec = (int)RTO % 1000 * 1000;
    }
}

void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int))
{
    signal(SIGALRM, sig_handler);            // set up signal handler, function passed to handle signal
    timer.it_interval.tv_sec = delay / 1000; // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000; // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}



int main(int argc, char **argv)
{
    gettimeofday(&time_init, 0); //get initial time
    writeToCSV(0 * 1000.0f + 0 / 1000.0f, window_size, ssthresh);

    int portno, len;
    char *hostname;
    char buffer[DATA_SIZE];
    FILE *fp;

    /* check command line arguments */
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL)
    {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* initialize server server details */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0)
    {
        fprintf(stderr, "ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    // Stop and wait protocol
    gettimeofday(&time_init, 0);
    init_timer(RTO, resend_packets);
    
    // Initialize a queue as buffer
    struct Queue buffer_queue;
    initQueue(&buffer_queue);
    while (1)
    {   /*
            While block for sending process
            - Condition for sending: when the packet in flight is less than window_size
        */
        while (size(&buffer_queue) < window_size){
            len = fread(buffer, 1, DATA_SIZE, fp);
            // end of file, break out of sending, keep receiving
            if (len <= 0){
                EOF_flag = 1;
                break;
            }
            // Make Packet
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = nextToSend_seqno;

            //Initialization wwhen buffer is empty
            if (isEmpty(&buffer_queue))
            {
                send_base = nextToSend_seqno;
                nextToAck_seqno = send_base + len;
                basepkt = sndpkt; 
                //we only start timer on oldest packet at the beginning
                start_timer();
            }
            
            //printf("Sending packet %d to %s\n", sndpkt->hdr.seqno, hostname);
            //send the data
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                       (const struct sockaddr *)&serveraddr, serverlen) < 0)
            {
                error("sendto");
            }

            nextToSend_seqno += len; //update pointer to the pkt to send next
            gettimeofday(&send_time, 0); // note the time of each pkt was sent
            sndpkt->send_time = send_time; //record individual send time
            enqueue(&buffer_queue, sndpkt); // buffer current packet
            printf("Sending pkt %d..\n", sndpkt->hdr.seqno);

        }
        /*
            Wait for ACK
            Receive Ack sequentially unless we receive 3 duplicate Ack -> fast retransmit
        */
        int duplicate = 0;
        
        do
        {
            // if we already received more than 3 ack, resend
            if (duplicate == 4){
                duplicate = 0;
                // Reset the timer for the newly sent pkt
                stop_timer();
                //mulplicative decrease when packet loss happens
                ssthresh = fmax(2, window_size / 2); 
                window_size = 1;
                struct timeval t1; //dummy var for counting
                gettimeofday(&t1,0);
                //printf("Window size: %d\n", window_size);
                writeToCSV(timedifference_msec(time_init, t1), window_size, ssthresh);

                resend_flg = 1; //signal retransmission happening
                backoff *= 2; //karn's algo
                RTO = backoff * RTO; //actual RTO after backoff
                if (RTO > upper_RTO){ //check if RTO reaches upper bound
                    RTO = upper_RTO;
                }
                //printf("Current RTO after resend: %lf\n", RTO);
                if (sendto(sockfd, peek(&buffer_queue), TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                           (const struct sockaddr *)&serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }
                init_timer(RTO, resend_packets);  
                start_timer(); 
               // break;
            }else if (duplicate > 1 && duplicate < 4){
                resend_flg = 1;
            }
            if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                         (struct sockaddr *)&serveraddr, (socklen_t *)&serverlen) < 0)
            {
                error("recvfrom");
            }
            resend_flg = 0; //if an ack is received, reset flg

            gettimeofday(&ack_time,0);

            backoff = 1;
            recvpkt = (tcp_packet *)buffer;
            duplicate++; // count dup pkt

            //printf("%d \n", get_data_size(recvpkt));
            printf("Ack %d is received!\n",recvpkt->hdr.ackno);
            assert(get_data_size(recvpkt) <= DATA_SIZE);
        } while (recvpkt->hdr.ackno < nextToAck_seqno); 
        //if we receive multiple duplicate ack for the basepkt, initiate resend


        /*
        "Window Sliding" handling:
        Every time window slides:
        1. Stop the timer for the previous send_base
        2. Dequeue until the front.seqno == recvpk->hdr.ackno
        3. set send_base to front.seqno
        4. start time for the front pkt
        5. If the queue is empty, terminate the program
        */

       stop_timer();
       //Slide the window if we still have pkt to send
       //And if we receive ack

        //Calculate RTT, RTO
        //RTT calculation based upon newest ack time and oldest unacked packet send time
        if (!resend_flg){   
            sample_rtt = (double)(ack_time.tv_sec - basepkt->send_time.tv_sec) * 1000.0 + (ack_time.tv_usec - basepkt->send_time.tv_usec) / 1000.0;
            estimated_rtt = (1-a) * estimated_rtt + a * sample_rtt;
            dev_rtt = (1-b) * dev_rtt + b * abs(estimated_rtt - sample_rtt);
            RTO = estimated_rtt + 4 * dev_rtt; 
        }
        RTO = backoff * RTO; //actual RTO after backoff
        if (RTO > upper_RTO){ //check if RTO reaches the limit
            RTO = upper_RTO;
        }
        //printf("Current RTO: %lf\n", RTO);

        //handle cum ack, dequeue as many packet base on ackno received
        int numDequeue = 0;
        while (!isEmpty(&buffer_queue) && (peek(&buffer_queue))->hdr.seqno < recvpkt->hdr.ackno){
            numDequeue ++;
            headpkt = dequeue(&buffer_queue);
            free(headpkt);
        }

        //Update pointers for maintaining window
        send_base = recvpkt->hdr.ackno;
        nextToAck_seqno = send_base + len;
        //printf("The buffer after dequeue: ");
        //printQueue(&buffer_queue);
        //printf("EOF flag is: %d\n", EOF_flag);

        //When we finishing sending && receiving ack for all pkt, terminate
        if ((EOF_flag ==1) && isEmpty(&buffer_queue)){
            VLOG(INFO, "End Of File has been reached");
            sndpkt = make_packet(0);
            sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
                   (const struct sockaddr *)&serveraddr, serverlen);
            return 0;
        }

        //Update oldest unack pkt
        basepkt = peek(&buffer_queue);
             
        //Update timer
        struct timeval curr; //dummy var for calculation
        gettimeofday(&curr, 0);
        float elasped = timedifference_msec(basepkt->send_time, curr);
        //represent the time remaining until the timeout from the timestamp of the oldest unACKed pkt.
        int els_int;
        els_int = (int)elasped;
        els_int = RTO - els_int;// time remaning till timeout from p2 timestamp

        timer.it_value.tv_sec = els_int / 1000; // sets an initial value
        timer.it_value.tv_usec = (els_int % 1000) * 1000;
        start_timer(); 
    
        //update window size
        if (!resend_flg){
            if (window_size < ssthresh){
                if(numDequeue == 1.0){
                    window_size += 1; //slow start implementation
                }else{
                    window_size += numDequeue; //deal with cum ack when 1 ack actually means acknowledgement of multiple pkt
                }
                //printf("Window size: %d\n", window_size);
                writeToCSV(timedifference_msec(time_init, curr), window_size, ssthresh);
            }else{
                if (calculated_cwnd == 0){
                    calculated_cwnd = (float)window_size; //initialize calculated_cwnd for the first time entering CA only
                }
                for(int i = 0; i < numDequeue; i++){
                    calculated_cwnd = calculated_cwnd + 1.0/(float)window_size; //deal with cum ack
                }
                window_size = floor(calculated_cwnd);
                //printf("Window size: %f\n", calculated_cwnd);
                writeToCSV(timedifference_msec(time_init, curr), calculated_cwnd, ssthresh);
            }
            //printf("Window size: %d, thresh: %d\n", window_size, ssthresh); 
        }
        resend_flg = 0;        
    }
}