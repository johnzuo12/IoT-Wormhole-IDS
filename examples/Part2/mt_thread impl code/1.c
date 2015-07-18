#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include "string.h"

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#include "net/rpl/rpl.h"
#define UDP_EXAMPLE_ID  190
//#include "debug.h"

#include "math.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

#define MAPPER_ADD_PACKETDATA(dest, source) \
  memcpy(dest, &source, sizeof(source)); dest += sizeof(source)

#define MAPPER_GET_PACKETDATA(dest, source) \
  memcpy(&dest, source, sizeof(dest)); source += sizeof(dest)

#define BROADCAST_PORT 10000

#define Neighbor_queue 7
#define Nodes_in_network 15
#define node_range 100
static short int rssi[100];

static struct uip_udp_conn *udp_bconn;
static struct uip_udp_conn *server_conn,*server_conn1;
static uip_ipaddr_t ipaddr;
static struct uip_ds6_addr *root_if;

static int total_nodes = 0;

struct Neighbor {
    uint8_t id;
    rpl_rank_t rank;
};


struct Node {
    uip_ipaddr_t ip;
    uint8_t parent_id;
    uint8_t id;
    short int x,y; 
    short int last_msg_instance;;
    rpl_rank_t rank,p_rank;
    struct Neighbor neighbor[Neighbor_queue];
    uint16_t neighbors;
};
struct Node nodes[Nodes_in_network];

static struct etimer nw_settle_timer, addmyinfo_wait;

/*---------------------------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/

void init_rssi(void)
{
rssi[0]=-10;	rssi[1]=-11;	rssi[2]=rssi[3]=-12;	rssi[4]=rssi[5]=-14;	rssi[6]=-15;	rssi[7]=rssi[8]=-16;
rssi[9]=-17;	rssi[10]=-18;	rssi[11]=-19;	rssi[12]=-20;	rssi[13]=-21; 	rssi[14]=-22;	rssi[15]=rssi[16]=-23;	rssi[17]=-24;	rssi[18]=-25;	rssi[19]=-26;	rssi[20]=-27;	rssi[21]=-28;	rssi[22]=rssi[23]=-29;	rssi[24]=-30;	rssi[25]=-32;	rssi[26]=-32;rssi[27]=-33;	rssi[28]=rssi[29]=-34;	rssi[30]=-35;	rssi[31]=-36;	rssi[32]=-37;	rssi[33]=-38;	rssi[34]=-39;	
rssi[35]=rssi[36]=-40; rssi[37]=-41;	rssi[38]=-42;	rssi[39]=-43;	rssi[40]=-44;	rssi[41]=-45;	rssi[42]=rssi[43]=-46;
rssi[44]=-47;	rssi[45]=-48;	rssi[46]=-49;	rssi[47]=-50;	rssi[48]=rssi[49]=-52;	rssi[50]=-52;	rssi[51]=-53;	rssi[52]=-54;	rssi[53]=-55;	rssi[54]=-56;	rssi[55]=rssi[56]=-57;	rssi[57]=-58;	rssi[58]=-59;	rssi[59]=-60;	rssi[60]=-61;
rssi[61]=-62;	rssi[62]=rssi[63]=-63;	rssi[64]=-64;	rssi[65]=-65;	rssi[66]=-66;	rssi[67]=-67;	rssi[68]=rssi[69]=-68;
rssi[70]=-69;	rssi[71]=-70;	rssi[72]=-71;	rssi[73]=-72;	rssi[74]=rssi[75]=rssi[76]=-74;	rssi[77]=-75;
rssi[78]=-76;	rssi[79]=-77;	rssi[80]=-78;	rssi[81]=-79;	rssi[82]=rssi[83]=-80;	rssi[84]=-81;	rssi[85]=-82;
rssi[86]=-83;	rssi[87]=-84;	rssi[88]=rssi[89]=-85;	rssi[90]=-86;	rssi[91]=-87;	rssi[92]=-88;	rssi[93]=-89;
rssi[94]=-90;	rssi[95]=rssi[96]=-91;	rssi[97]=-92;	rssi[98]=-93;	rssi[99]=-94;	
}



double calculate_distance (double x1,double x2,double y1 ,double y2)
{
	
    printf("x1 x2 y1 y2 %lf %lf %lf %lf \n",x1,x2,y1,y2); 	
    double  distance_x = x1-x2;
    double distance_y = y1- y2;
    double x= (distance_x * distance_x);
    double y= (distance_y * distance_y);
    return sqrt( x+y );
}

static void
send_unicast(uip_ipaddr_t *dest, char *buf, int size)
{
    uip_ipaddr_copy(&server_conn->ripaddr, dest);
    uip_udp_packet_send(server_conn, buf, size);
    uip_create_unspecified(&server_conn->ripaddr);
}

static void
send_broadcast(char *buf, int size)
{
    uip_create_linklocal_allnodes_mcast(&udp_bconn->ripaddr);
    uip_udp_packet_send(udp_bconn, buf, size);
    uip_create_unspecified(&udp_bconn->ripaddr);
}



void validate_parent_child(uint8_t id, uint8_t *appdata)
{
	printf("In parent child validation Node ID %u \n", id);
	int i, j, k;short int code =0;
	int pc_distance;uint8_t intermediate_node;
	for(i=0;i<=total_nodes;i++) // i points to child
	{	
		if(nodes[i].id==id)
			break;
	}
	for(j=0;j<=total_nodes;j++) // j points to parent
	{	
		if(nodes[i].parent_id==nodes[j].id)
			break;
	}
	printf("In validate: node %u parent %u \n",nodes[i].id,nodes[j].id);
	pc_distance = (int)calculate_distance((double)nodes[i].x,(double)nodes[j].x,(double)nodes[i].y,(double)nodes[j].y);
	printf("Distance %d \n",pc_distance);
	if(pc_distance > node_range)
	{
		printf("attack occurred \n");
		//return 1;
		MAPPER_GET_PACKETDATA(intermediate_node,appdata);
		unsigned char buf[5],buf2[5], *buf_p;
		buf_p=buf;	code=3;	//victim code
		MAPPER_ADD_PACKETDATA(buf_p,code);		
		MAPPER_ADD_PACKETDATA(buf_p,id); // sending to parent its child node and it is victim
		uip_udp_packet_sendto(server_conn, buf, sizeof(buf), &nodes[j].ip, UIP_HTONS(10000+(int)nodes[j].ip.u8[15]));
		uip_create_unspecified(&server_conn->ripaddr);
		
		if(nodes[j].ip.u8[15]!=intermediate_node)//checking parent node is not intermediate node
		{ 
			
			for(k=0;k<=total_nodes;k++) // k points to intermediate node
			{	
				if(nodes[k].id==intermediate_node)
					break;
			}
		
			buf_p=buf2;	code=1;	//helper code
			MAPPER_ADD_PACKETDATA(buf_p,code);		
			MAPPER_ADD_PACKETDATA(buf_p,id); // sending to helper to send whome
			MAPPER_ADD_PACKETDATA(buf_p,nodes[j].ip.u8[15]); // adding parent ID
			uip_udp_packet_sendto(server_conn, buf2, sizeof(buf2), &nodes[k].ip, UIP_HTONS(10000+(int)nodes[k].ip.u8[15]));
			uip_create_unspecified(&server_conn->ripaddr);
			printf("parent is not intermediate node: sending to helper node buf %u\n",sizeof(buf2));
			printf("Next call to attack detection\n");
		}else {	

			buf_p=buf2;	code=1;	//helper code
			MAPPER_ADD_PACKETDATA(buf_p,code);		
			MAPPER_ADD_PACKETDATA(buf_p,id); // sending to helper to send whome
			MAPPER_ADD_PACKETDATA(buf_p,nodes[j].ip.u8[15]); // adding parent ID
			uip_udp_packet_sendto(server_conn, buf2, sizeof(buf2), &nodes[j].ip, UIP_HTONS(10000+(int)nodes[j].ip.u8[15]));
			uip_create_unspecified(&server_conn->ripaddr);			

		/*	//case for wormhole attack if parent node is intermediate node sending to child node
			buf_p=buf2;	code=3;	//Victim code
			MAPPER_ADD_PACKETDATA(buf_p,code);		
			MAPPER_ADD_PACKETDATA(buf_p,nodes[j].ip.u8[15]); // adding parent ID
			uip_udp_packet_sendto(server_conn, buf2, sizeof(buf2), &nodes[i].ip, UIP_HTONS(10000+(int)nodes[i].ip.u8[15]));
			uip_create_unspecified(&server_conn->ripaddr);
			printf("parent is intermediate node sending to child node %u buf %u\n",nodes[i].ip.u8[15],sizeof(buf2)); */
		}	
	}			
}

static void add_node(uint8_t *appdata)
{

    MAPPER_GET_PACKETDATA(nodes[total_nodes].ip,appdata);
    MAPPER_GET_PACKETDATA(nodes[total_nodes].x,appdata);
    MAPPER_GET_PACKETDATA(nodes[total_nodes].y,appdata);

    nodes[total_nodes].id=nodes[total_nodes].ip.u8[15];

    nodes[total_nodes].last_msg_instance=0;	

    nodes[total_nodes].ip.u16[0]=0xaaaa;
    PRINT6ADDR(&nodes[total_nodes].ip);
    printf(" Node %u X: %d Y: %d added %d \n",nodes[total_nodes].id,nodes[total_nodes].x,nodes[total_nodes].y,total_nodes);
    total_nodes++;

    //uip_ipaddr_t *dest = &nodes[total_nodes-1].ip;  
    //send_unicast(dest, buf5, sizeof(buf5));	
    //printf("Distance %d \n",calculate_distance(1,10,1,900));
    //uip_udp_packet_sendto(server_conn, buf5, sizeof(buf5), &nodes[total_nodes-1].ip, UIP_HTONS(10000+(int)nodes[total_nodes-1].ip.u8[15]));
}

void update_info(uint8_t *appdata)
{
    int i,j;
    uint8_t nodeid=0; 
    short int msg_instance=0;
    uint8_t temp_id;	

    MAPPER_GET_PACKETDATA(nodeid,appdata);
    MAPPER_GET_PACKETDATA(temp_id,appdata);
    MAPPER_GET_PACKETDATA(msg_instance,appdata);		
    
    for(i=0; i<total_nodes; i++)
    {
        if(nodeid==nodes[i].id)
            break;
    }
    	

    if(!msg_instance == 100*nodeid)  //checking reprocessing of the info received by broadcast
	{if(msg_instance <= nodes[i].last_msg_instance)
		return;}
    		
    nodes[i].last_msg_instance = msg_instance;	
    printf("Message instance : %d ", msg_instance);		
	
    MAPPER_GET_PACKETDATA(nodes[i].rank,appdata);
    MAPPER_GET_PACKETDATA(nodes[i].parent_id,appdata);
    MAPPER_GET_PACKETDATA(nodes[i].p_rank,appdata);
    MAPPER_GET_PACKETDATA(nodes[i].neighbors,appdata);

    printf("Node %u, rank %u,parent %u PRank %u, Numof_nbr %d ",nodes[i].id, nodes[i].rank, nodes[i].parent_id, nodes[i].p_rank,nodes[i].neighbors);
    //storing neighbor
    for(j=0; j<nodes[i].neighbors; j++)
    {
        MAPPER_GET_PACKETDATA(nodes[i].neighbor[j].id,appdata);
        MAPPER_GET_PACKETDATA(nodes[i].neighbor[j].rank,appdata);
        printf("Nbr %u R %u ",nodes[i].neighbor[j].id,nodes[i].neighbor[j].rank);
    }
    printf("\n");
}

static void broadcast_recv(void)
{
/*    static uint8_t *appdata=NULL;
    static short int code=0;
    printf("broadcast received\n" );
    if(uip_newdata()) {

        appdata = (uint8_t *) uip_appdata;
        MAPPER_GET_PACKETDATA(code,appdata);
        if(code==2) {
            //adding neighbor info and ranks
	    printf("updating node info by broadcast \n" );	
            update_info(appdata);
	    if(etimer_expired(&nw_settle_timer))
	    {	
		if(!validate_parent_child(UIP_IP_BUF->srcipaddr.u8[15]))
			printf("Next call to attack detection\n");
	    } 
			
        }
    }   */
}



static void
tcpip_handler(void)
{
    static uint8_t *appdata=NULL,nodeid;
    static short int code=0;

    //printf("size of %d %d %d \n ",sizeof(int),sizeof(short int), sizeof(char));
    if(uip_newdata()) {

        appdata = (uint8_t *) uip_appdata;
        MAPPER_GET_PACKETDATA(code,appdata);
        if(code==1)
        {
            //adding node and location info
            add_node(appdata);
        } else if(code==2) {
            //adding neighbor info and ranks
            update_info(appdata);
	    //printf("updating node info by unicast \n");
	    if(etimer_expired(&nw_settle_timer))	//if nw settle timer expires then only chk for attack
	    {	
		printf("timer expired\n");
		MAPPER_GET_PACKETDATA(nodeid,appdata);
		validate_parent_child(nodeid,appdata);
	    } 
			
        }
    }   
    //uip_udp_packet_sendto(server_conn, "pavan", sizeof("pavan"), &UIP_IP_BUF->srcipaddr, UIP_HTONS(12345));
    //uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    //uip_udp_packet_send(server_conn, "Reply", sizeof("Reply"));
    //uip_create_unspecified(&server_conn->ripaddr);
}

static void
create_dag(void)
{

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
    //uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
    root_if = uip_ds6_addr_lookup(&ipaddr);
    if(root_if != NULL) {
        static rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("created a new RPL dag with ID:");
        PRINT6ADDR(&dag->dag_id);
        printf("\n");
    } else {
        PRINTF("failed to create a new RPL DAG\n");
    }

}

static void addmyinfo(void)
{
	nodes[0].id=1;
	nodes[0].x=73;
	nodes[0].y=19;
	uip_ip6addr(&nodes[0].ip, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
	nodes[0].rank=256;
	nodes[0].parent_id=1;
	nodes[0].p_rank=256;
	nodes[0].neighbors=uip_ds6_nbr_num();

	printf("Node %u, rank %u,parent %u PRank %u, Numof_nbr %d ",nodes[0].id, nodes[0].rank, nodes[0].parent_id, nodes[0].p_rank,nodes[0].neighbors);

	uip_ds6_nbr_t *nbr=NULL;
	int i=0;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {  
	nodes[0].neighbor[i].id=nbr->ipaddr.u8[15];	
	printf(" Nbr %d: %u",i,nodes[0].neighbor[i].id);i++;
	} 
	printf("\n");
	total_nodes++;
}

PROCESS_THREAD(udp_server_process, ev, data)
{

    	
    PROCESS_BEGIN();

    create_dag();

    server_conn = udp_new(NULL, NULL, NULL);
    if(server_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn, UIP_HTONS(2345));

    server_conn1 = udp_new(NULL, NULL, NULL);
    if(server_conn1 == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn1, UIP_HTONS(2346));

    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
           UIP_HTONS(server_conn->rport));
    //udp_bconn = udp_broadcast_new(UIP_HTONS(BROADCAST_PORT), NULL);
    
    init_rssi();	
    	
    PRINTF("Listen port: 2345, TTL=%u\n", server_conn->ttl);
    etimer_set(&nw_settle_timer, 90 * CLOCK_SECOND);//network setting time
    etimer_set(&addmyinfo_wait, 30 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&addmyinfo_wait));	
    addmyinfo();		
    while(1) {
		
        PROCESS_YIELD();
        if(ev==tcpip_event)
        {	printf("Tcpip event\n");
                tcpip_handler();
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
