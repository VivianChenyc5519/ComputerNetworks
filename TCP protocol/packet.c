#include <stdlib.h>
#include"packet.h"
//packet has all the data everything set to zero, initialze everything with 0 to receive new data
static tcp_packet zero_packet = {.hdr={0}};
/*
 * create TCP packet with header and space for data of size len
 */
tcp_packet* make_packet(int len)
{
    tcp_packet *pkt;
    pkt = malloc(TCP_HDR_SIZE + len + TIME_SIZE);//memory allocation

    *pkt = zero_packet;
    pkt->hdr.data_size = len;
    pkt->send_time.tv_sec = 0;
    pkt->send_time.tv_usec = 0;
    return pkt;
}

int get_data_size(tcp_packet *pkt)
{
    return pkt->hdr.data_size;
}

