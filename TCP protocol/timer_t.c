/*
The code illustrates the concept of shifting the timer to the oldest unACKed segment
in a simplified congestion control scenario. 


Scenario:

Timeout Value: The timeout value is set to 500 milliseconds. This means that if a
packet isn't acknowledged within 500 ms of being sent, it's considered lost, and
a retransmission is needed.

Packet Transmission:

Packet 1: Sent at a relative time of 50 ms. When Packet 1 is sent, a timer is
started to track its timeout.

Packet 2: Sent at a relative time of 100 ms.

Packet 3: Sent at a relative time of 200 ms.

ACK for Packet 1: An acknowledgment (ACK) for Packet 1 is received at a relative
time of 350 ms. This means that Packet 1 was successfully received by the receiver.

Oldest UnACKed Packet: After receiving the ACK for Packet 1, the sender considers
Packet 1 as acknowledged and successfully received. Therefore, the oldest unACKed
packet in the sender's window is now Packet 2.

Shifting Timer to Packet 2: To ensure that the sender's timer is tracking the
oldest unACKed segment, the sender shifts the timer to Packet 2. However, this
shift needs to consider the time already elapsed since Packet 2 was sent.

Calculating Remaining Time for Packet 2 Timeout: To calculate the remaining time
for Packet 2's timeout, the sender does the following:
  - Calculates the elapsed time from when Packet 2 was sent (100 ms) to the current
    time (350 ms).
  - Calculates the remaining time until the expected timeout for Packet 2, which
    is 500 ms - elapsed time = 500 ms - 250 ms = 250 ms.


Shifting the Timer: The sender then sets the timer to expire in 250 ms and starts
the timer. This ensures that Packet 2 will be retransmitted if it's not
acknowledged within the next 250 ms. 

Timeout Handling: If the timer expires (reaches 0), the sender will consider
Packet 2 as having timed out (not acknowledged within the 500 ms timeout window),
and it will trigger a retransmission of Packet 2.

Importance of Shifting the Timer:
Without shifting the timer, if the sender simply started a new timer after
receiving the ACK for Packet 1, it would allow Packet 2 a full timeout of 500 ms.
However, this would not be correct because Packet 2 has already been in transit for
350 ms when the ACK for Packet 1 arrived. Shifting the timer to Packet 2 with the
remaining time of 250 ms accurately reflects the time Packet 2 has been in transit
and avoids unnecessarily waiting for the full 500 ms timeout.

So, shifting the timer to the oldest unACKed segment ensures that the sender
accurately tracks the timeout for the segment that has been in transit the longest,
preventing unnecessary delays in retransmitting packets. This also helps avoid 
unnecessary retransmissions that could worsen congestion.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

// to compile this code, you need to link math library by using '-lm': gcc -o output code.c -lm


FILE *csv;
int window_size = 64;
struct timeval current_packet_time; 
struct itimerval timer;
struct timeval time_init; //for intial time of prgram
sigset_t sigmask; //block and unblock signals during timer management

void resend_packets(int);
void start_timer();
void stop_timer();
void init_timer(int, void(int));
float timedifference_msec(struct timeval, struct timeval);


int main(int argc, char **argv)
{
	gettimeofday(&time_init, 0);

	/*
	A timer timer is set with an initial value of 500 milliseconds, and it's
	associated with a signal handler (resend_packets) that will be triggered when
	the timer expires.
	*/
	init_timer(500, resend_packets);

	// assuming 
	int p1 = 1;
	int p2 = 1;

	//timestamps storing when packets p1 and p2 are sent
	struct timeval p1_time;
	struct timeval p2_time;
	
	printf("conceptually sending the packet p1 \n");
	gettimeofday(&p1_time, 0); // note the time of p1 sent
	
	//set the current packet time to the time when p1 is sent
	current_packet_time = p1_time;
	
	//start the timer
	start_timer();



	//simulate sending packet p2
	printf("conceptually sending the packet p2 \n");
	gettimeofday(&p2_time, 0); // note the time of p2 sent

	//wait for some time to simulate the reception of ACK for p1
	printf("sleep for 100 milliseconds \n");
	usleep(100000);

	printf("conceptually assumed ack for the packet p1 is received \n");
	
	//stopping the timer associated with p1
	stop_timer();


	//prepare to start a timer for p2, considering its own sending time.
	current_packet_time = p2_time;
    

	//calculate the elapsed time since p2 was sent using timedifference_msec
    struct timeval t1; // dummy for calculation
    gettimeofday(&t1, 0);
	float elasped = timedifference_msec(p2_time, t1);
	printf("elasped milliseconds for packet p2 : %f\n", elasped);
	
	//represent the time remaining until the timeout from the timestamp of p2.
	int els_int;
	els_int = (int)elasped;
	els_int = 500 - els_int;// time remaning till timeout from p2 timestamp
	printf("remaining time in milliseconds for packet p2 : %d\n", els_int);

	// it will be better to check if els_int is positive
	if (els_int > 0)
	{
	//If els_int (remaining time for p2 timeout) is positive, the timer is set
	//to this remaining time, and it's started.
	timer.it_value.tv_sec = els_int / 1000; // sets an initial value
    timer.it_value.tv_usec = (els_int % 1000) * 1000;
	start_timer();
	init_timer(1000, resend_packets);
	
	//After a 500 ms sleep, the timer for p2 will expire and be stopped.
	printf("sleep for 500 milliseconds \n");
	usleep(500000); // in midst of it, the timeout of packet 2 will happen
	stop_timer();
	}
	else
	{
	/*
	If els_int is non-positive (e.g., packet already timed out), the resend_packets
	function can be directly called.
	*/
	resend_packets(SIGALRM);

	}


	// csv = fopen("../cwnd.csv", "w");
    // if (csv == NULL)
    // {
    //     printf("Error opening csv\n");
    //     return 1;
    // }

	// gettimeofday(&t1, 0);
    // fprintf(csv, "%f,%d\n", timedifference_msec(time_init, t1), (int)congestion_window_size, (int)ssthresh);


	// fclose(csv);

}


void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {

		printf("conceptually resending the packets \n");


		struct timeval t1;
        gettimeofday(&t1, 0); 

		printf("Timeout happened after : %f \n", timedifference_msec(t1, current_packet_time));

        // plot new window size

        //fprintf(csv, "%f,%d\n", timedifference_msec(t1, time_init), (int)window_size);

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
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000; // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000; // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

float timedifference_msec(struct timeval t0, struct timeval t1)
{
    return fabs((t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
}

