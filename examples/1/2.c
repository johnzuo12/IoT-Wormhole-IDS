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
extern rpl_instance_t instance_table[];


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

    uip_ipaddr_t *parent_ipaddr=NULL;
    PRINTF("\nDATA send to %d \n", server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1]);

    rpl_parent_t *preferred_parent=NULL;
    rpl_dag_t *current_dag=NULL;

    int j=0,i;
    rpl_instance_t * instance_id;
    for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
        if(instance_table[i].used && instance_table[i].current_dag->joined) {
            current_dag = instance_table[i].current_dag;
            instance_id = &instance_table[i];
        }
    }

    if(current_dag != NULL) {
        parent_ipaddr=rpl_get_parent_ipaddr(current_dag->preferred_parent);
        //uip_debug_ipaddr_print(parrent_ipaddr);
        //printf("\n Rank0 %u",current_dag->rank);
        //printf("\nPARENT RANK %d ",current_dag->preferred_parent->rank);
        //printf("\n DAG %u",current_dag->version);
        //printf("\n Instance_ID %u",instance_id->instance_id);
    }

    uip_ds6_nbr_t *nbr=NULL;
    uip_ipaddr_t * myip=NULL;
    myip = &uip_ds6_get_link_local(ADDR_PREFERRED)->ipaddr;
    int num = uip_ds6_nbr_num();
    int sizeof_buf = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) 
		    +sizeof(uint16_t) +sizeof(uint8_t) + sizeof(uint16_t)+ sizeof(int)
		    + num * ( sizeof(uint8_t) + sizeof(uint16_t) );
		    // node + version + instance id + own rank + parent rank + numof_nbr*(nbr+rank)	
    unsigned char buf[sizeof_buf];
    unsigned char * buf_p = buf;
    
    printf("\nint size :%d\n",sizeof(int));
    MAPPER_ADD_PACKETDATA(buf_p, myip->u8[15]);
    //MAPPER_ADD_PACKETDATA(buf_p, current_dag->version);
    //MAPPER_ADD_PACKETDATA(buf_p, instance_id->instance_id);
    MAPPER_ADD_PACKETDATA(buf_p, current_dag->rank);
    MAPPER_ADD_PACKETDATA(buf_p, parent_ipaddr->u8[15]);
    MAPPER_ADD_PACKETDATA(buf_p, current_dag->preferred_parent->rank); 
    MAPPER_ADD_PACKETDATA(buf_p, num);

    rpl_rank_t rank;rpl_parent_t *p;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
        //p=find_parent_any_dag_any_instance(&nbr->ipaddr);
	p=rpl_find_parent_any_dag(instance_id, &nbr->ipaddr);
        if(p==NULL)
		printf("parent not found");
        MAPPER_ADD_PACKETDATA(buf_p,nbr->ipaddr.u8[15]);
	//rank = rpl_get_parent_rank(uip_ds6_nbr_get_ll(nbr));
        MAPPER_ADD_PACKETDATA(buf_p,p->rank);
        printf("neighbor : %02x rank %u\n",nbr->ipaddr.u8[15],p->rank);
    } 


/* for(p = list_head(instance_table[i].dag_table[j].parents); p !=NULL; p = list_item_next(p)) {
              
parent_ipaddr=rpl_get_parent_ipaddr(p);
              MAPPER_ADD_PACKETDATA(buf_p, parent_ipaddr->u8[15]);

              MAPPER_ADD_PACKETDATA(buf_p, p->rank);

              PRINTF("Nbr %u got rank %d \n",parent_ipaddr->u8[15], p->rank);
            }*/
    

    printf("Node %02x X: %d, Y: %d R %u Parent %u Parent Rank %u Bufsize %d\n",myip->u8[15], node_loc_x, node_loc_y,current_dag->rank,parent_ipaddr->u8[15],current_dag->preferred_parent->rank, sizeof_buf);
    //printf("\nBuf %s \n",buf);
    uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));

    //char buf2[40];
    //sprintf(buf2, "%02x %d %d",myip->u8[15],node_loc_x, node_loc_y);
    //uip_udp_packet_sendto(client_conn, buf, strlen(buf), &server_ipaddr, UIP_HTONS(2346));

}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
    int i;
    uint8_t state;

    PRINTF("Client IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
            PRINTF("\n");
            /* hack to make address "final" */
            if (state == ADDR_TENTATIVE) {
                uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
            }
        }
    }
}
/*---------------------------------------------------------------------------*/


void
set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    /* set server address */
    uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
}

PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer periodic;
    static struct ctimer backoff_timer;
#if WITH_COMPOWER
    static int print = 0;
#endif

    PROCESS_BEGIN();

    set_global_address();

    PRINTF("UDP client process started\n");

    print_local_addresses();

    /* new connection with remote host */
    client_conn = udp_new(NULL, UIP_HTONS(2345), NULL);
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
