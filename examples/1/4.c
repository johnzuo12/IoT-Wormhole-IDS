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
#define MAX_PAYLOAD_LEN		30

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
extern uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];
extern rpl_instance_t instance_table[];

extern uip_ds6_defrt_t uip_ds6_defrt_list[];

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
  static int seq_id;
  char buf[MAX_PAYLOAD_LEN];
  uip_ipaddr_t *pipaddr;
  seq_id++;
  PRINTF("\nDATA send to %d Packet %d\n",
         server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1], seq_id);
  
  rpl_parent_t *preferred_parent;
  rpl_dag_t *dag;
  dag = rpl_get_any_dag();
  if(dag != NULL) {
    preferred_parent = dag->preferred_parent;
    if(preferred_parent != NULL) {
//PRINTF("parent: ");

pipaddr=rpl_get_parent_ipaddr(preferred_parent);

//uip_debug_ipaddr_print(pipaddr);

	int j=0,i=0;
            // My rank
	dag->rank=500;
	PRINTF("\n Rank0 %d",dag->rank);
        
        // preferred parent
        PRINTF("\nParent: ");
	pipaddr=rpl_get_parent_ipaddr(instance_table[i].dag_table[j].preferred_parent);
	uip_debug_ipaddr_print(pipaddr);

	instance_table[i].dag_table[j].rank=500;
	PRINTF("\nPARENT RANK %d ",instance_table[i].dag_table[j].preferred_parent->rank);
	//PRINT6ADDR(pipaddr);		
	//PRINTF("\nOwn rank %d ",instance_table[i].dag_table[j].rank);
}
}

	uip_ipaddr_t * myip;
        myip = &uip_ds6_get_link_local(ADDR_PREFERRED)->ipaddr;
        
  
sprintf(buf, " X: %d, Y: %d", node_loc_x, node_loc_y);
//sprintf(buf, "Packet %d from client %02x X: %d, Y: %d",seq_id,myip->u8[15], node_loc_x, node_loc_y);
  uip_udp_packet_sendto(client_conn, buf, strlen(buf), &server_ipaddr, UIP_HTONS(2345));

sprintf(buf, "Pavan %d from client %02x",seq_id,myip->u8[15]);
uip_udp_packet_sendto(client_conn, buf, strlen(buf), &server_ipaddr, UIP_HTONS(2346));
	

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
      ctimer_set(&backoff_timer, CLOCK_SECOND * 5, send_packet, NULL);

    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
