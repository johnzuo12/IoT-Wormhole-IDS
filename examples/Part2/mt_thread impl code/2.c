#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "sys/ctimer.h"

#include <stdio.h>
#include <string.h>
#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
//#include "cc2420.c"

#include "net/uip-ds6-nbr.h"
#include "net/rpl/rpl-dag.c" //for tracking parent change

#include "net/netstack.h"
#define UDP_EXAMPLE_ID  190
#include "rpl.h"
//#include "rpl-dag.c"
#include "node-id.h"

#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/button-sensor.h"
#include "dev/radio-sensor.h"
#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"
#include "debug.h"

#include "net/mac/sicslowmac.c"
#include "sys/mt.h"


#define SEND_INTERVAL		(60 * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))
#define BROADCAST_PORT 10000

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
//#define UIP_UDP_BUF          ((struct uip_udp_hdr *)&uip_buf[UIP_LLIPH_LEN])
//#define UIP_ICMP_BUF            ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])


#define MAPPER_ADD_PACKETDATA(dest, source) \
  memcpy(dest, &source, sizeof(source)); dest += sizeof(source)

#define MAPPER_GET_PACKETDATA(dest, source) \
  memcpy(&dest, source, sizeof(dest)); source += sizeof(dest)

static struct uip_udp_conn *udp_bconn;
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr, myip;
extern rpl_instance_t instance_table[];

static uint8_t nbrs[10], attacker_set=0;
int msg_instance=0;
static int num_nbrs;
uint16_t myrank;
static struct etimer wait_timer, monitor_timer; 

int head=-1;tail=-1;

struct pq
{
	unsigned char buf[50];
	uint8_t dest;
	int psize;
};

struct pq pbuf[10];
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

/*
addq(uint8_t *buf_p, int size, uint8_t dest)
{
	if(head==10)
		head=-1;
	memcpy(pbuf[++head], buf_p, size);
	pbuf[head].psize=size;
	pbuf[head].dest=dest;		
	if(tail==-1)
		tail++;
}

sendq()
{
	unsigned char buf[pbuf[tail].size], *buf_p;
	uip_ds6_nbr_t *nbr=NULL;
	uip_ipaddr_t tempadd;

	memcpy(buf_p, pbuf[tail].buf, pbuf[tail].size);
	if(dest==1)
	{
		uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
		uip_create_unspecified(&client_conn->ripaddr);    
	}
	else
	{
		for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {	
		if(dest==nbr->ipaddr.u8[15])
		{        
			uip_ipaddr_copy(&tempadd, &nbr->ipaddr);
			//tempadd.u16[0]=0xaaaa;                
			uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
			uip_create_unspecified(&client_conn->ripaddr);
			printf("nbr info sent to %u\n",tempadd.u8[15]);
		}

		}
	}
	tail++;	 
}
*/

static void
send_unicast(uip_ipaddr_t *dest, char *buf, int size)
{
    uip_ipaddr_copy(&client_conn->ripaddr, dest);
    uip_udp_packet_send(client_conn, buf, size);
    uip_create_unspecified(&client_conn->ripaddr);
}

static void
send_broadcast(unsigned char *buf, int size)
{
    printf("In send broadcast: \n");
    uip_create_linklocal_allnodes_mcast(&udp_bconn->ripaddr);
    uip_udp_packet_send(udp_bconn, buf, size);
    uip_create_unspecified(&udp_bconn->ripaddr);
}

static void forward_packet(uint8_t *appdata)
{
    uint8_t nodeid;
    int code=2;
    uint8_t temp_id=0;
    printf("Forwarding by broadcast\n");	
	//appdata = (uint8_t *) uip_appdata;
	//MAPPER_GET_PACKETDATA(code,appdata);
	MAPPER_GET_PACKETDATA(nodeid,appdata);	
        MAPPER_GET_PACKETDATA(temp_id,appdata);
	printf("In broad cast recv Code %d osndr %u myid %u \n", code, nodeid, myip.u8[15]);

	printf("Data size %d \n",uip_datalen());
	unsigned char buf[uip_datalen()], *buf_p; 
	buf_p=buf;  
	MAPPER_ADD_PACKETDATA(buf_p,code); //1 
	MAPPER_ADD_PACKETDATA(buf_p,nodeid);//1		original sender			
	MAPPER_ADD_PACKETDATA(buf_p,myip.u8[15]);//1    intermediate sender
	//MAPPER_ADD_PACKETDATA(buf_p,appdata);
	memcpy(buf_p, appdata, uip_datalen() - 3);

	//uip_create_unspecified(&udp_bconn->ripaddr);
	//uip_create_unspecified(&client_conn->ripaddr);      			
	uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
	uip_create_unspecified(&client_conn->ripaddr);    
	printf("Broadcasted Data unicasted buf size %d \n", sizeof(buf));

    //printf("received a broadcast of %d bytes: %s\n", uip_datalen(),uip_appdata);
}


static int is_nbr(uint8_t a)
{
	int i=0;
	printf(" nbr %u myip %u a %u \n",nbrs[i],myip.u8[15],a);
	for(i=0;i<num_nbrs;i++)
	{		
		if(nbrs[i]==a || myip.u8[15]==a)
			return 1;		
		
	}
	if(nbrs[i]!=a && myip.u8[15]!=a)
			return 0;
	
}


static int snd_nbr_info(void)
{
	int num = uip_ds6_nbr_num();
	if(num==1)
	{	printf("Only one neighbor so returning \n");
		return 0;	
	}
        unsigned char buf4[num +10], *buf_p; 
	buf_p=buf4; int i=8;
	uip_ipaddr_t tempadd;

    	uip_ds6_nbr_t *nbr=NULL;
	MAPPER_ADD_PACKETDATA(buf_p,i);
	MAPPER_ADD_PACKETDATA(buf_p,num);

    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
        
        MAPPER_ADD_PACKETDATA(buf_p,nbr->ipaddr.u8[15]);
       	
	}
	uip_ipaddr_copy(&tempadd, &UIP_IP_BUF->srcipaddr);
        //tempadd.u16[0]=0xaaaa;                
	uip_udp_packet_sendto(client_conn, buf4, sizeof(buf4), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
	uip_create_unspecified(&client_conn->ripaddr);
	printf("nbr info sent to %u\n",tempadd.u8[15]);
	return 1;
}

static void select_attacker(uint8_t *appdata)
{

	//printf("Selecting attacker\n");
	int num,i,j,k; uip_ipaddr_t tempadd,temp2;
	MAPPER_GET_PACKETDATA(num,appdata);
	uint8_t a; unsigned char buf[5],buf2[5], *buf_p;

	for(i=0;i<num;i++)
	{
		
		MAPPER_GET_PACKETDATA(a,appdata);	
		//printf("nbr of %u chk for %u\n",UIP_IP_BUF->srcipaddr.u8[15],a);

		uip_ipaddr_copy(&temp2, &UIP_IP_BUF->srcipaddr);
		if(a!=1)
		if(UIP_IP_BUF->srcipaddr.u8[15]!=myip.u8[15])
		if(a!=myip.u8[15])
		if(!is_nbr(a))
		{	
  			
			k=3;j=9;
			buf_p=buf2;
			MAPPER_ADD_PACKETDATA(buf_p,j);
			MAPPER_ADD_PACKETDATA(buf_p,k);			
			//uip_ip6addr(&server_ipaddr, 0xfe80, 0, 0, 0,0x0212, 0x7401, 1, 0x0101);
			tempadd.u16[0]=0xaaaa; tempadd.u16[1]=0;tempadd.u16[2]=0; tempadd.u16[3]=0;
			tempadd.u16[4]=0x1202; tempadd.u8[10]=0x74;tempadd.u8[11]=a;tempadd.u8[12]=0;tempadd.u8[13]=a;tempadd.u8[14]=a;
			tempadd.u8[15]=a;
			uip_udp_packet_sendto(client_conn, buf2, sizeof(buf2), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
			uip_create_unspecified(&client_conn->ripaddr);	
			printf("attacker 3 is %u\n",tempadd.u8[15]);

			
 			k=2;buf_p=buf;
			MAPPER_ADD_PACKETDATA(buf_p,j);
			MAPPER_ADD_PACKETDATA(buf_p,k);						
			//temp2.u16[0]=0xaaaa;                
			uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &temp2, UIP_HTONS(10000+(int)temp2.u8[15]));
			uip_create_unspecified(&client_conn->ripaddr);	
			printf("attacker 2 is %u\n",temp2.u8[15]);

			
			attacker_set=1;
			attacker=1;		
			attack_flag=1;
			dis_output(NULL);
			setreg(17,0);
		}
	}
		
}

static void help(uint8_t *appdata)
{
	unsigned char buf[5], *buf_p;int i=3;//victim code
	uint8_t dest;	
	buf_p=buf;

	uip_ds6_nbr_t *nbr=NULL;
    	uip_ipaddr_t tempadd;

	MAPPER_ADD_PACKETDATA(buf_p,i);
	MAPPER_GET_PACKETDATA(dest,appdata);

	memcpy(buf_p, appdata, 1);
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) { 
	if(dest==nbr->ipaddr.u8[15])
	{       
		uip_ipaddr_copy(&tempadd, &nbr->ipaddr);
		uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
		uip_create_unspecified(&client_conn->ripaddr);
		printf("In hepl: sent to %u\n",tempadd.u8[15]);	
		return;
	}
	}
	printf("In help: neighbor not found\n");
} 

static void monitoring(uint8_t *appdata)
{
	uint8_t a;
	//etimer_set(&monitor_timer, CLOCK_SECOND*10);
	
	MAPPER_GET_PACKETDATA(a,appdata);
	printf("My colligue %u \n",a);
	monitor_target = a;
	monitor=1;
	int i=0;
	//for(i=0;i<10;i++)
	{
	//uip_udp_packet_sendto(client_conn, "hi", sizeof("hi"), &server_ipaddr, UIP_HTONS(2345));
	//uip_create_unspecified(&client_conn->ripaddr);
	}	
}

static void
tcpip_handler(void)
{
 /*
    char *str;
    uip_ipaddr_t tadd;
    printf("In tcphandler\n");
    if(uip_newdata()) {
        str = uip_appdata;
        str[uip_datalen()] = '\0';
        printf("DATA recv '%s'\n", str);
        //uip_ip6addr(&tadd, 0xaaaa, 0, 0, 0, 0x0212, 0x7402, 0x0002, 0x0202);
        //uip_udp_packet_sendto(client_conn, "from client ", sizeof("from client "), &tadd, UIP_HTONS(12345));
    }
*/
    
        uint8_t *appdata=NULL,a;int code=0;
        if(uip_newdata()) {

    	appdata = (uint8_t *) uip_appdata;
    	MAPPER_GET_PACKETDATA(code,appdata);
    	switch(code)
    	{
    		case 1://helping packet
	    	printf("Helper message \n");
			help(appdata);
    		break;
    		case 2://adding neighbor info and ranks
    			forward_packet(appdata);
    		break;
		case 3://victim packet
    		printf("I am Victim\n");
			monitoring(appdata);
    		break;
		case 4://monitoring node
		
    		break;
		case 5://Selent packet
    		//upade_info(appdata);
    		break;
		case 7://Nbr info req
		snd_nbr_info();
    		break;
		case 8://nbr_info process
		if(!attacker_set)    		
			select_attacker(appdata); 
    		break;
		case 9://Wormhole deactive
		
    		MAPPER_GET_PACKETDATA(code,appdata);
		if(code==2)
		{printf("Attacker 2\n");attack_flag=1;attacker=2;setreg(17,0);}
		if(code==3)	
		{printf("Attacker 3\n");attack_flag=1;attacker=3;dis_output(NULL);setreg(17,0);}
    		break;
    	}
        }
}




/*---------------------------------------------------------------------------*/
static void
sendpacket(int mode)
{

    static uip_ipaddr_t *parent_ipaddr=NULL,tempadd;
    static rpl_dag_t *current_dag=NULL;
    	
    static int i=0;
    static rpl_instance_t * instance_id=NULL;
    for(i = 0; i < RPL_MAX_INSTANCES; ++i) {
        if(instance_table[i].used && instance_table[i].current_dag->joined) {
            current_dag = instance_table[i].current_dag;
            instance_id = &instance_table[i];
            break;
        }
    }

    if(current_dag != NULL) {
        parent_ipaddr=rpl_get_parent_ipaddr(current_dag->preferred_parent);
    }else return; //if current dag is null return

    if(!parent_ipaddr->u8[15])// if parent is null return
	return;	    
	 		
    uip_ds6_nbr_t *nbr=NULL;
    int num = uip_ds6_nbr_num();
    int sizeof_buf = sizeof(int) + sizeof(uint8_t)  + sizeof(uint8_t) + sizeof(int) + sizeof(uint16_t) +sizeof(uint8_t) + sizeof(uint16_t)+ sizeof(int) + num * ( sizeof(uint8_t) + sizeof(uint16_t) );
    // code + node + id_of_intermediate + msg_instance + own rank + parent + rank + numof_nbr*(nbr+rank)
    unsigned char buf[sizeof_buf];
    unsigned char * buf_p = buf;
    i=2;myrank=current_dag->rank;
    MAPPER_ADD_PACKETDATA(buf_p, i);
    MAPPER_ADD_PACKETDATA(buf_p, myip.u8[15]);

	
    if(mode)		//for first time setting 
    msg_instance = 100*myip.u8[15]; 	
        		
    MAPPER_ADD_PACKETDATA(buf_p, myip.u8[15]);   //added to track limited boradcasting intermediate node id
    MAPPER_ADD_PACKETDATA(buf_p, msg_instance);        //added to track limited boradcasting
    msg_instance++;	

    
    MAPPER_ADD_PACKETDATA(buf_p, current_dag->rank);
    MAPPER_ADD_PACKETDATA(buf_p, parent_ipaddr->u8[15]);
    MAPPER_ADD_PACKETDATA(buf_p, current_dag->preferred_parent->rank);
    MAPPER_ADD_PACKETDATA(buf_p, num);

    printf(" %02x R %u P %u PR %u Bufsize %d msg_instance %d ",myip.u8[15],current_dag->rank,parent_ipaddr->u8[15],current_dag->preferred_parent->rank, sizeof_buf, msg_instance-1);

    num_nbrs=0;
    rpl_parent_t *p=NULL; 
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
        //p=find_parent_any_dag_any_instance(&nbr->ipaddr);
        p=rpl_find_parent_any_dag(instance_id, &nbr->ipaddr);
	
        MAPPER_ADD_PACKETDATA(buf_p,nbr->ipaddr.u8[15]);
        //rank = rpl_get_parent_rank(uip_ds6_nbr_get_ll(nbr));
        MAPPER_ADD_PACKETDATA(buf_p,p->rank);
        printf("No of nbrs %d : %02x rank %u ",num,nbr->ipaddr.u8[15],p->rank);
	
	nbrs[num_nbrs]=nbr->ipaddr.u8[15];
	num_nbrs++;

    }
    printf("\n");

    if(mode)	
    {
	uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
	uip_create_unspecified(&client_conn->ripaddr);    
	parent_change=0;
	printf("parent change info send via unicast 6BR\n");		
    }
    else
    {   
	

	//uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));//temporaryly aadded
	//uip_create_unspecified(&client_conn->ripaddr);  
	
	//send_broadcast(buf,sizeof(buf)); ///*
	//uip_create_unspecified(&udp_bconn->ripaddr);

	int j=0;
	for(i=0,j=0;i<=num_nbrs*3;i++,j=0)
		for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {        
		uip_ipaddr_copy(&tempadd, &nbr->ipaddr);
		j++;
		if(nbr->ipaddr.u8[15]!=myip.u8[15] && i==j){
			uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
			printf("send to %u \n",tempadd.u8[15]);
			uip_create_unspecified(&client_conn->ripaddr);
		}
	}	
	printf("parent change info send via broadcast 6BR\n");		
    }		 	
}
/*---------------------------------------------------------------------------*/


static void
print_local_addresses(void)
{
    static int i;
    static uint8_t state;

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

static void
set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    /* set server address */
    uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
    //uip_ip6addr(&server_ipaddr, 0xfe80, 0, 0, 0,0x0212, 0x7401, 1, 0x0101);

}
static void send_xy(void)
{

    int i=1, x=node_loc_x, y=node_loc_y, sizeof_buf = sizeof(int) + sizeof(uip_ipaddr_t) + sizeof(int) +sizeof(int);
    unsigned char buf[sizeof_buf];
    unsigned char * buf_p = buf;

    MAPPER_ADD_PACKETDATA(buf_p, i);
    MAPPER_ADD_PACKETDATA(buf_p, myip);
    MAPPER_ADD_PACKETDATA(buf_p, x);
    MAPPER_ADD_PACKETDATA(buf_p, y);

    PRINT6ADDR(&myip);
    printf(" %02x X %u Y %u Bufsize %d \n",myip.u8[15],node_loc_x,node_loc_y, sizeof_buf);
    //send_unicast(&server_ipaddr,buf,sizeof(buf));

    uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
    uip_create_unspecified(&client_conn->ripaddr);
    //uip_udp_packet_sendto(client_conn, "from client ", sizeof("from client "), &server_ipaddr, UIP_HTONS(12345));
}




void attack_init(void)
{
	if(num_nbrs<=1)
	{printf("Insufficient neighbors can't create attack \n"); return;
	}	
        unsigned char buf4[4], *buf_p;
	buf_p=buf4; int i=7;	//nbr info code
	uip_ds6_nbr_t *nbr=NULL;
    	uip_ipaddr_t tempadd;
	MAPPER_ADD_PACKETDATA(buf_p,i);
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {        
	uip_ipaddr_copy(&tempadd, &nbr->ipaddr);
        //tempadd.u16[0]=0xaaaa;                
	uip_udp_packet_sendto(client_conn, buf4, sizeof(buf4), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
	uip_create_unspecified(&client_conn->ripaddr);
	printf("nbr req sent to %u\n",tempadd.u8[15]);
    }
}


PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer start_timer, send_timer;
    static int flag=1;int i=0;
    static struct mt_thread sending_thread, attackinit_thread;	

    PROCESS_BEGIN();

    SENSORS_ACTIVATE(button_sensor);
    SENSORS_ACTIVATE(radio_sensor);

    set_global_address();

    printf("UDP client process started\n");

    print_local_addresses();

	myip=uip_ds6_get_link_local(ADDR_PREFERRED)->ipaddr;

    /* new connection with remote host */
    client_conn = udp_new(NULL, NULL, NULL);
    if(client_conn == NULL) {
        printf("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(10000+(int)myip.u8[15]));

    udp_bconn = udp_broadcast_new(UIP_HTONS(BROADCAST_PORT),tcpip_handler);
    //uip_create_unspecified(&udp_bconn->ripaddr);	
    if(udp_bconn == NULL) {
        printf("No UDP broadcast connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
	
    printf("Created a connection with the server ");
    PRINT6ADDR(&client_conn->ripaddr);
    printf(" local/remote port %u/%u\n", UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

    etimer_set(&start_timer, 60 * CLOCK_SECOND);//network setting time
    etimer_set(&send_timer, CLOCK_SECOND * ((int)myip.u8[15]+60+40));//node check/send parent info
    char buf[3]="hi";


    mt_init();
    mt_start(&sending_thread, sendpacket, 0);
    mt_start(&attackinit_thread, attack_init, NULL);	
    while(1) {

        PROCESS_YIELD();

        //NETSTACK_RDC.off(0);
        //NETSTACK_MAC.off(0);
        //NETSTACK_RADIO.off();
        //button_sensor::value(0);

        if(ev==tcpip_event)
        {
			
	    tcpip_handler();	                  
        }
	if(rssi_stored==5)// if got 5 rssi value from parent or child
	{
		monitor=0;
		monitor_target=0;
		for(i=0;i<5;i++)
			printf("Monitered value %d \n",mrssi[i]);
		rssi_stored=0;
	}
        if((ev==sensors_event) && (data == &button_sensor)) {            
	if(attack_flag)      
	{
		printf("Attack deactivated\n");   
		attack_flag=0;
		attacker=0;
		attacker_set=0;
		dis_output(NULL);
	}else {
		printf("Attack activated\n");   
		mt_exec(&attackinit_thread);
	}	
        }
        if((ev==sensors_event) && (data == &radio_sensor))
        {
            printf("Radio value %d",radio_sensor.value(0));
        }
        if(etimer_expired(&send_timer))
	{
		//uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
		//uip_create_unspecified(&client_conn->ripaddr); 
		//if(myip.u8[15]==4)
		//sendpacket(0);
		etimer_set(&send_timer, CLOCK_SECOND*(myip.u8[15]+60));
		if(parent_change)        // send only at parent change event
		{	            
	            if(!attack_flag)	//  this is not attacker and
		    {	
			mt_exec(&sending_thread);
			//sendpacket(0);	//  send nbr info by broadcast 0 for broadcast 
			parent_change=0;
			printf("Thread initiated\n");
		    }
	        }
	}
        if(etimer_expired(&start_timer) && flag==1)
        {
            flag=0;
            send_xy();
	    etimer_set(&start_timer, CLOCK_SECOND*(myip.u8[15]+1));
	    PROCESS_WAIT_UNTIL(etimer_expired(&start_timer));
	    send_broadcast("hi",sizeof("hi"));	
	    sendpacket(1);	// 0 means send by unicast	
	    	
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
