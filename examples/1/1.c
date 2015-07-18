#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#include "net/rpl/rpl.h"

#include "debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

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

static struct uip_udp_conn *server_conn, *server_conn1;
static uip_ipaddr_t ipaddr;
static rpl_dag_t *dag;
struct uip_ds6_addr *root_if;
/*---------------------------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
    int numof_nbr;
    uint8_t nodeid, version,instance_id, parent_node;
    uint16_t node_rank, node_parent_rank;
    uint8_t *appdata;
    if(uip_newdata()) {

        appdata = (uint8_t *) uip_appdata;
        MAPPER_GET_PACKETDATA(nodeid,appdata);
        //MAPPER_GET_PACKETDATA(version,appdata);
        //MAPPER_GET_PACKETDATA(instance_id,appdata);
        MAPPER_GET_PACKETDATA(node_rank,appdata);
        MAPPER_GET_PACKETDATA(parent_node,appdata);
        MAPPER_GET_PACKETDATA(node_parent_rank,appdata);
        MAPPER_GET_PACKETDATA(numof_nbr,appdata);

        uint8_t nbr_id[numof_nbr];
        uint16_t nbr_rank[numof_nbr];
        

        printf("\n Node %u, rank %u,parent %u PRank %u, Numof_nbr %d, ",nodeid,node_rank,parent_node,node_parent_rank,numof_nbr);
        int i;
        for(i=0; i<numof_nbr; i++) {
            MAPPER_GET_PACKETDATA(nbr_id[i],appdata);
            MAPPER_GET_PACKETDATA(nbr_rank[i],appdata);
	    //MAPPER_GET_PACKETDATA(a,appdata);
            //MAPPER_GET_PACKETDATA(b,appdata);
                        
printf("Nbr %d: %u nbr_rank %u, ",i,nbr_id[i],nbr_rank[i]);
        }
	printf("\n");
    }
}
void
create_dag()
{
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
    /* uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr); */
    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
    root_if = uip_ds6_addr_lookup(&ipaddr);
    if(root_if != NULL) {
        rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("created a new RPL dag with ID:");
        PRINT6ADDR(&dag->dag_id);printf("\n");
    } else {
        PRINTF("failed to create a new RPL DAG\n");
    }
    /*
      uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
      uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
      uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

      dag = rpl_set_root(RPL_DEFAULT_INSTANCE,
                         &uip_ds6_get_global(ADDR_PREFERRED)->ipaddr);
      if(dag != NULL) {
        uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("Created a new RPL dag with ID: ");
        PRINT6ADDR(&dag->dag_id);
        PRINTF("\n");
      }*/
}



PROCESS_THREAD(udp_server_process, ev, data)
{

    PROCESS_BEGIN();

    create_dag();

    server_conn = udp_new(NULL, UIP_HTONS(12345), NULL);
    udp_bind(server_conn, UIP_HTONS(2345));

    server_conn1 = udp_new(NULL, UIP_HTONS(12345), NULL);
    udp_bind(server_conn1, UIP_HTONS(2346));

    PRINTF("Listen port: 2345, TTL=%u\n", server_conn->ttl);

    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event)
            tcpip_handler();
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
