#ifndef __IDS_COMMON_H__
#define __IDS_COMMON_H__

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "string.h"

//#define DEBUG 1
//#include "net/uip-debug.h"

#include "net/rpl/rpl.h"
#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])


#define attacker1 209
#define attacker2 219
#define attacker3 118
#define attacker4 13
#define attack_tym 5

#define BROADCAST_PORT 10000
#define x_loc -66//2//-66//91
#define y_loc 65//125//65//75
#define Neighbor_queue 10
#define node_range 100
#define Nodes_in_network 20
//sending nbr info after send_time sec

#if Nodes_in_network > 1 && Nodes_in_network <= 10
#define one_node_set_time 16 // 2.5 min
#define send_tym 180
#endif

#if Nodes_in_network > 10 && Nodes_in_network <= 20
#define one_node_set_time 8 //2.6 min
#define send_tym 224
#endif

#if Nodes_in_network > 20 && Nodes_in_network <= 30
#define one_node_set_time 5//6 //3min
#define send_tym 300//300
#endif

#if Nodes_in_network > 30 && Nodes_in_network <= 40
#define one_node_set_time 5//3.33 min
#endif


#define rssi_wait_tym 20 + 6*Nodes_in_network
#define reset_tym 20 + 6*Nodes_in_network +20
#define nwsettym Nodes_in_network*one_node_set_time

//#include "lib/random.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "sys/etimer.h"

#include <stdio.h>

#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
//#include "cc2420.c"

#include "net/uip-ds6-nbr.h"
#include "net/rpl/rpl-dag.c" //for tracking parent change


//#define DEBUG DEBUG_PRINT
//#include "net/uip-debug.h"
//#include "debug.h"

//#define SEND_INTERVAL		(60 * CLOCK_SECOND)
//#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

//#define UIP_UDP_BUF          ((struct uip_udp_hdr *)&uip_buf[UIP_LLIPH_LEN])
//#define UIP_ICMP_BUF            ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

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


#endif
