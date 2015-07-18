#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "sys/ctimer.h"
#ifdef WITH_COMPOWER
#include "powertrace.h"
#endif
#include <stdio.h>
#include <string.h>
#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
#include "net/uip-nd6.h"
#include "net/uip-ds6-nbr.h"
#include "net/rpl/rpl-dag.c"

#include "net/netstack.h"

#include "rpl.h"

#include "node-id.h"

#include "contiki-lib.h"
#include "contiki-net.h"


#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#ifndef PERIOD
#define PERIOD 60
#endif

#define START_INTERVAL		(15 * CLOCK_SECOND)
#define SEND_INTERVAL		(PERIOD * CLOCK_SECOND)
//#define SEND_TIME		(random_rand() % (SEND_INTERVAL))
//#define SEND_TIME		(random_rand() % (SEND_INTERVAL))


#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF          ((struct uip_udp_hdr *)&uip_buf[UIP_LLIPH_LEN])
#define UIP_ICMP_BUF            ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

/**
 * Copy the contents from source (needs to be a normal type) to dest, which
 * needs to be a pointer and increment dest by the size of source.
 *
 * Usefull for writing data to a buffer, the inverse of MAPPER_GET_PACKETDATA
 */
#define MAPPER_ADD_PACKETDATA(dest, source) \
  memcpy(dest, &source, sizeof(source)); dest += sizeof(source)
/**
 * Copy the contents from source (needs to be a pointer) to dest, which
 * needs to be normal data type. Increase source with the size of dest.
 *
 * Usefull for reading data from a buffer, the inverse of MAPPER_ADD_PACKETDATA
 */
#define MAPPER_GET_PACKETDATA(dest, source) \
  memcpy(&dest, source, sizeof(dest)); source += sizeof(dest)

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;



/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{

    if(uip_newdata()) {

    }
}

/*---------------------------------------------------------------------------*/
static void
send_packet(void *ptr)
{
    char buf2[40];
    sprintf(buf2, "hi pavan");
    uip_udp_packet_sendto(client_conn, buf2, strlen(buf2), &server_ipaddr, UIP_HTONS(2346));

}
/*---------------------------------------------------------------------------*/


PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer periodic;
    static struct ctimer backoff_timer;
#if WITH_COMPOWER
    static int print = 0;
#endif

    PROCESS_BEGIN();

    PRINTF("UDP client process started\n");



    /* new connection with remote host */
    client_conn = udp_new(NULL, UIP_HTONS(2346), NULL);
    if(client_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(12345));

    PRINTF("Created a connection with the server ");
    PRINT6ADDR(&client_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n",
           UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));


    etimer_set(&periodic, SEND_INTERVAL);
    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event) {
            tcpip_handler();
        }

        if(etimer_expired(&periodic)) {
            etimer_reset(&periodic);
            ctimer_set(&backoff_timer, CLOCK_SECOND * 1, send_packet, NULL);

        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
