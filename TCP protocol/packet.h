//enumeration, readable format to define constant
enum packet_type {
    DATA,
    ACK,
};

//tcp_header file
typedef struct {
    int seqno;//sequence number
    int ackno;//ack number
    int ctr_flags;//control flags, if it is a ack/syn packet
    int data_size;//data payload
}tcp_header;

#define MSS_SIZE    1500//maximum size
#define UDP_HDR_SIZE    8
#define IP_HDR_SIZE    20
#define TCP_HDR_SIZE    sizeof(tcp_header)
#define TIME_SIZE    sizeof(struct timeval)
#define DATA_SIZE   (MSS_SIZE - TCP_HDR_SIZE - UDP_HDR_SIZE - IP_HDR_SIZE)
typedef struct {
    tcp_header  hdr;
    char    data[0];//way to declare flexible array
    struct timeval send_time;
}tcp_packet;

tcp_packet* make_packet(int seq);
int get_data_size(tcp_packet *pkt);