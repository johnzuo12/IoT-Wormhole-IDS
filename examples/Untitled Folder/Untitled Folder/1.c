
#include "math.h"
#include "ids-common.h"

static signed char rssi[100];
static uip_ipaddr_t myip;
static struct uip_udp_conn *udp_bconn;
static struct uip_udp_conn *server_conn;
static uip_ipaddr_t ipaddr;
static struct uip_ds6_addr *root_if;
static int total_nodes = 0,Nodes_in_nw=Nodes_in_network,rssi_timer_flag=0;
uint8_t monitor_target=0;
static signed char mrssi[3];
static uint8_t last_target=0,suspect_c=0,suspect_send=0,last_informer=0;
static int monitor=0,send_timer_flag=0,reset_timer_flag=0;
static int rssi_stored=0;

struct Neighbor {
    uint8_t id;
};


struct Node {
    uip_ipaddr_t ip;
    uint8_t parent_id, sender_nodeid;
    uint8_t id;
    short int x,y;
    short int last_msg_instance;
    struct Neighbor neighbor[Neighbor_queue];
    short int neighbors;
};
static struct Node nodes[Nodes_in_network];

static struct etimer nw_settle_timer, addmyinfo_wait;

static int distance[Nodes_in_network][Nodes_in_network],rssi_flag=0;
struct vtm
{
    uint8_t id, suspect;
    int mrssi;
    int is_rssi_stored;
};
static struct vtm victim[15];
static int victim_count=0,reset_time=reset_tym,rssi_wait_time=rssi_wait_tym,nwsettime=nwsettym;

static uint8_t current_victim[2], attack_processing=0,node_to_v=0;
/*---------------------------------------------------------------------------*/
PROCESS(udp_server_process, "Up");
PROCESS(distance_process, "Dp");
PROCESS(monitor_process, "Mp");
PROCESS(postrssi_process, "PRp");
PROCESS(validatepc_process, "Vp");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/

void init_rssi(void)
{
    rssi[0]=-10;
    rssi[1]=-11;
    rssi[2]=rssi[3]=-12;
    rssi[4]=rssi[5]=-14;
    rssi[6]=-15;
    rssi[7]=rssi[8]=-16;
    rssi[9]=-17;
    rssi[10]=-18;
    rssi[11]=-19;
    rssi[12]=-20;
    rssi[13]=-21;
    rssi[14]=-22;
    rssi[15]=rssi[16]=-23;
    rssi[17]=-24;
    rssi[18]=-25;
    rssi[19]=-26;
    rssi[20]=-27;
    rssi[21]=-28;
    rssi[22]=rssi[23]=-29;
    rssi[24]=-30;
    rssi[25]=-32;
    rssi[26]=-32;
    rssi[27]=-33;
    rssi[28]=rssi[29]=-34;
    rssi[30]=-35;
    rssi[31]=-36;
    rssi[32]=-37;
    rssi[33]=-38;
    rssi[34]=-39;
    rssi[35]=rssi[36]=-40;
    rssi[37]=-41;
    rssi[38]=-42;
    rssi[39]=-43;
    rssi[40]=-44;
    rssi[41]=-45;
    rssi[42]=rssi[43]=-46;
    rssi[44]=-47;
    rssi[45]=-48;
    rssi[46]=-49;
    rssi[47]=-50;
    rssi[48]=rssi[49]=-52;
    rssi[50]=-52;
    rssi[51]=-53;
    rssi[52]=-54;
    rssi[53]=-55;
    rssi[54]=-56;
    rssi[55]=rssi[56]=-57;
    rssi[57]=-58;
    rssi[58]=-59;
    rssi[59]=-60;
    rssi[60]=-61;
    rssi[61]=-62;
    rssi[62]=rssi[63]=-63;
    rssi[64]=-64;
    rssi[65]=-65;
    rssi[66]=-66;
    rssi[67]=-67;
    rssi[68]=rssi[69]=-68;
    rssi[70]=-69;
    rssi[71]=-70;
    rssi[72]=-71;
    rssi[73]=-72;
    rssi[74]=rssi[75]=rssi[76]=-74;
    rssi[77]=-75;
    rssi[78]=-76;
    rssi[79]=-77;
    rssi[80]=-78;
    rssi[81]=-79;
    rssi[82]=rssi[83]=-80;
    rssi[84]=-81;
    rssi[85]=-82;
    rssi[86]=-83;
    rssi[87]=-84;
    rssi[88]=rssi[89]=-85;
    rssi[90]=-86;
    rssi[91]=-87;
    rssi[92]=-88;
    rssi[93]=-89;
    rssi[94]=-90;
    rssi[95]=rssi[96]=-91;
    rssi[97]=-92;
    rssi[98]=-93;
    rssi[99]=-94;

    current_victim[0] = current_victim[1]=0;
    victim[0].is_rssi_stored=victim[1].is_rssi_stored=0;
}



static void
send_broadcast(unsigned char *buf, int size)
{
    //printf("In send B: \n");
    uip_create_linklocal_allnodes_mcast(&udp_bconn->ripaddr);
    uip_udp_packet_send(udp_bconn, buf, size);
    uip_create_unspecified(&udp_bconn->ripaddr);
}

double calculate_distance (int x1,int x2,int y1 ,int y2)
{

    //printf("x1 x2 y1 y2 %lf %lf %lf %lf \n",x1,x2,y1,y2);
    double distance_x = x1-x2;
    double distance_y = y1-y2;
    double x= (distance_x * distance_x);
    double y= (distance_y * distance_y);
    return sqrt( x+y );
}

PROCESS_THREAD(validatepc_process, ev, data)
{

    static int i, j, k;
    static short int code =0, nbrs;
    int pc_distance;
    static uint8_t  id;
    static unsigned char buf[8],buf2[8], *buf_p;
    static struct etimer wait_timer;

    PROCESS_BEGIN();
    id=node_to_v;
    for(i=0; i<total_nodes; i++) // i points to child
    {
        if(nodes[i].id==id)
            break;
    }
    nbrs=nodes[i].neighbors;
    //printf("In validate: node %u nbrs %d \n",nodes[i].id,nbrs);

    for(k=0; k<nbrs; k++)
        for(j=0; j<total_nodes; j++) // j points to parent
        {


            if(nodes[i].neighbor[k].id!=nodes[i].id)
                if(nodes[i].neighbor[k].id==nodes[j].id)
                {
                    //pc_distance = (int)calculate_distance(nodes[i].x,nodes[j].x,nodes[i].y,nodes[j].y);
                    //printf("In validate: node %u nbr %u ",nodes[i].id,nodes[j].id);
                    //printf("Diatance %d\n",pc_distance);
		    //pc_distance = distance[i][j];	

                    if(distance[i][j]>node_range)
                    {
			//if(last_informer==node_to_v)	
			//	return;//to avoid attack processing from same and same node only
		        //else
			//	last_informer=node_to_v;// keep track last attack processing node	

			attack_processing=1;
                        current_victim[0] = nodes[i].id;
                        current_victim[1] = nodes[j].id;
                        
                        printf("In validate: node %u nbr %u \n",nodes[i].id,nodes[j].id);
                        printf("Distance %d\n",distance[i][j]);
                        printf("Attack occurred \n");

                        buf_p=buf2;
                        code=3;	//Victim code
                        MAPPER_ADD_PACKETDATA(buf_p,code);
                        MAPPER_ADD_PACKETDATA(buf_p,nodes[j].id); // for u ?
                        MAPPER_ADD_PACKETDATA(buf_p,nodes[i].id); // adding parent ID
                        // not sending packet to itself
                        uip_udp_packet_sendto(server_conn, buf2, sizeof(buf2), &nodes[j].ip, UIP_HTONS(10000+(int)nodes[j].ip.u8[15]));
                        uip_create_unspecified(&server_conn->ripaddr);

                        etimer_set(&wait_timer, CLOCK_SECOND*(2));
                        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));

                        buf_p=buf;
                        code=3;	//victim code
                        MAPPER_ADD_PACKETDATA(buf_p,code);
                        MAPPER_ADD_PACKETDATA(buf_p,nodes[i].id); // for u ?
                        MAPPER_ADD_PACKETDATA(buf_p,nodes[j].id);//sending to parent its child node and it is victim
                        uip_udp_packet_sendto(server_conn, buf, sizeof(buf), &nodes[i].ip, UIP_HTONS(10000+(int)nodes[i].ip.u8[15]));
                        uip_create_unspecified(&server_conn->ripaddr);
			
			printf("packet sent to %u %u and ",nodes[i].ip.u8[15],nodes[j].ip.u8[15]);
                       
			etimer_set(&wait_timer, CLOCK_SECOND*(2));
                        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));
			/**************************************************************************************************/
			if(nodes[i].sender_nodeid!=nodes[i].id){
			buf_p=buf;
                        code=4;	//victim msg forwarding code
                        MAPPER_ADD_PACKETDATA(buf_p,code);
                        MAPPER_ADD_PACKETDATA(buf_p,nodes[i].id); // for u ?                        
			MAPPER_ADD_PACKETDATA(buf_p,nodes[j].id);//sending to parent its child node and it is victim
			
			for(j=0; j<total_nodes; j++) 
			{
			    if(nodes[j].id==nodes[i].sender_nodeid)
			    	break;
			}
		                        
			uip_udp_packet_sendto(server_conn, buf, sizeof(buf), &nodes[j].ip, UIP_HTONS(10000+(int)nodes[j].ip.u8[15]));
                        uip_create_unspecified(&server_conn->ripaddr);
			printf("%u %u\n",nodes[j].ip.u8[15],nodes[i].sender_nodeid);
			}
			/************************************************************************************************/
                        reset_timer_flag=1;
                        rssi_timer_flag=1;
                        rssi_flag=1;

                        if(nodes[i].id==1 || nodes[j].id==1)
                        {
                            monitor_target=nodes[i].id;
                            process_start(&monitor_process, NULL);
                        }

                        PROCESS_EXIT();

                    }
                }

        }

    if(nodes[j].id==0)
    {
        //printf("nbr not found \n");
        PROCESS_EXIT();
    }
    PROCESS_EXIT();
    PROCESS_END();
}

static void add_node(uint8_t *appdata)
{

    MAPPER_GET_PACKETDATA(nodes[total_nodes].ip,appdata);
    MAPPER_GET_PACKETDATA(nodes[total_nodes].x,appdata);
    MAPPER_GET_PACKETDATA(nodes[total_nodes].y,appdata);

    nodes[total_nodes].id=nodes[total_nodes].ip.u8[15];

    nodes[total_nodes].last_msg_instance=0;
    nodes[total_nodes].neighbors=0;
    nodes[total_nodes].ip.u16[0]=0xaaaa;
    printf("%d: Node %u X: %d Y: %d  \n",total_nodes+1,nodes[total_nodes].id,nodes[total_nodes].x,nodes[total_nodes].y);
    //PRINT6ADDR(&nodes[total_nodes].ip);
    total_nodes++;

}

int update_info(uint8_t *appdata)
{
    int i,j,new_node_add_flag=0;
    short int new_nbrs=0;
    uint8_t nodeid=0,sender_nodeid=0;
    short int msg_instance=0;

    MAPPER_GET_PACKETDATA(sender_nodeid,appdata);//skipping sender node
    MAPPER_GET_PACKETDATA(nodeid,appdata);//info owner
    MAPPER_GET_PACKETDATA(msg_instance,appdata);

    for(i=0; i<total_nodes; i++)
    {
        if(nodeid==nodes[i].id)
            break;
    }
    if(nodes[i].id==0)
    {
        printf("Adding new node %u %d\n",nodeid,i+1);
        nodes[i].id=nodeid;
        total_nodes++;
        msg_instance=1;

        nodes[i].ip.u16[0]=0xaaaa;
        nodes[i].ip.u16[1]=0;
        nodes[i].ip.u16[2]=0;
        nodes[i].ip.u16[3]=0;
        nodes[i].ip.u16[4]=0x1202;
        nodes[i].ip.u8[10]=0x74;
        nodes[i].ip.u8[11]=nodeid;
        nodes[i].ip.u8[12]=0;
        nodes[i].ip.u8[13]=nodeid;
        nodes[i].ip.u8[14]=nodeid;
        nodes[i].ip.u8[15]=nodeid;
        new_node_add_flag=1;
        //PRINT6ADDR(&nodes[total_nodes].ip);
        nodes[i].neighbors=0;
    }
    nodes[i].sender_nodeid=sender_nodeid;
    printf("Mid : %d %d ", msg_instance,nodes[i].last_msg_instance);
    // if(!msg_instance == 100*nodeid)  //checking reprocessing of the info received by broadcast
    if(msg_instance == nodes[i].last_msg_instance)
    {
        printf("old msg instance returning\n");
        return 2;
    }

    nodes[i].last_msg_instance = msg_instance;

    MAPPER_GET_PACKETDATA(nodes[i].parent_id,appdata);
    MAPPER_GET_PACKETDATA(new_nbrs,appdata);

    nodes[i].neighbors=new_nbrs;
    printf("Node %u sender %u, P %u, Nbrs: %d ",nodes[i].id, nodes[i].sender_nodeid, nodes[i].parent_id,nodes[i].neighbors);
    //storing neighbor
    for(j=0; j<nodes[i].neighbors; j++)
    {
        MAPPER_GET_PACKETDATA(nodes[i].neighbor[j].id,appdata);
        printf(" %u ",nodes[i].neighbor[j].id);
    }
    printf("\n");
    if(new_node_add_flag)
    {
        MAPPER_GET_PACKETDATA(nodes[i].x,appdata);
        MAPPER_GET_PACKETDATA(nodes[i].y,appdata);
        printf("X %d Y %d \n",nodes[i].x,nodes[i].y);
    }

    return 1;
}


void rssi_received(uint8_t *appdata)
{
    uint8_t sender, suspect;
    int i=0;
    signed char rssi_val1,rssi_val2,rssi_val3;

    MAPPER_GET_PACKETDATA(sender,appdata);
    MAPPER_GET_PACKETDATA(suspect,appdata);
    MAPPER_GET_PACKETDATA(rssi_val1,appdata);
    MAPPER_GET_PACKETDATA(rssi_val2,appdata);
    MAPPER_GET_PACKETDATA(rssi_val3,appdata);
    printf("RSSI %u %u %d %d %d from %u \n",sender,suspect,rssi_val1,rssi_val2,rssi_val3,UIP_IP_BUF->srcipaddr.u8[15]);
    if(current_victim[0]==sender)
    {
        if(victim[0].is_rssi_stored==1)
        {
            printf("Rssi already stored\n");
            return;
        }
        victim[0].id=sender;
        victim[0].suspect=suspect;
        victim[0].mrssi=(int)(rssi_val1 + rssi_val2+rssi_val3)/3;
        victim[0].is_rssi_stored=1;
        return;
    }
    if(current_victim[1]==sender)
    {
        if(victim[1].is_rssi_stored==1)
        {
            printf("Rssi already stored\n");
            return;
        }
        victim[1].id=sender;
        victim[1].suspect=suspect;
        victim[1].mrssi=(int)(rssi_val1 + rssi_val2+rssi_val3)/3;
        victim[1].is_rssi_stored=1;
        return;
    }
    if(victim_count) {
        for(i=0; i<victim_count; i++)
        {
            if(sender==victim[i+2].id && victim[i+2].is_rssi_stored==1) {
                printf("Rssi already stored\n");
                return;
            }

        }

        victim[2+victim_count].id=sender;
        victim[2+victim_count].suspect=suspect;
        victim[2+victim_count].mrssi=(int)(rssi_val1 + rssi_val2+rssi_val3)/3;
        victim[2+victim_count].is_rssi_stored=1;
        victim_count++;
        return;
    } else
    {
        victim_count++;
        victim[2].id=sender;
        victim[2].suspect=suspect;
        victim[2].mrssi=(int)(rssi_val1 + rssi_val2+rssi_val3)/3;
        victim[2].is_rssi_stored=1;
        return;
    }

}


PROCESS_THREAD(monitor_process, ev, data)
{
    static uint8_t a;
    static int n;
    static unsigned char *buf_p, buf[10];
    static struct etimer wait_timer;
    static short int code=3;	//victim code
    PROCESS_BEGIN();

    a=monitor_target;

    if(a==0)
        PROCESS_EXIT();

    buf_p = buf;//code+dest+collouge
    MAPPER_ADD_PACKETDATA(buf_p,code);
    MAPPER_ADD_PACKETDATA(buf_p,a);
    MAPPER_ADD_PACKETDATA(buf_p,myip.u8[15]); // sending to parent its child node and it is victim


    if(last_target==a)
    {
        printf("last mntr %u same ret\n",last_target);
        PROCESS_EXIT();
    }

    printf("Monitoring for %u last target %u \n",monitor_target,last_target);
    monitor=1;

    last_target=a;

    reset_timer_flag = send_timer_flag=1;

    for(n=0; n<9; n++) {

        send_broadcast(buf,sizeof(buf));

        etimer_set(&wait_timer, CLOCK_SECOND*2);
        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));

        printf("packet sent to %u last_target %u\n",monitor_target,last_target);

    }
    //printf("Monitor process exit\n");
    PROCESS_EXIT();
    PROCESS_END();
}

void suspect_monitor(uint8_t a,uint8_t b)
{
    int i=0,j=0,k=0;
    //printf("In suspect monitor\n");

    reset_timer_flag = send_timer_flag=1;

    for(i=0; i<nodes[0].neighbors; i++) {

        if(nodes[0].neighbor[i].id==a)
            j=1;
        if(nodes[0].neighbor[i].id==b)
            k=1;
    }
    if(k==0 && j==0)
    {
        printf("monitoring for unknown\n");
        printf("RSSI %d\n",packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45);
        mrssi[rssi_stored]=packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
        rssi_stored++;
        monitor=0;
        monitor_target=0;
        return;
    }
    else
    {
        if(j==0 && k==1) {
            monitor_target=a;
            last_target=a;
            monitor=1;
            rssi_stored=0;
            printf("monitoring for %u\n",a);
            return;
        }

        if(k==0 && j==1) {
            monitor_target=b;
            last_target=b;
            monitor=1;
            rssi_stored=0;
            printf("monitoring for %u\n",b);
            return;
        }
    }
}


static void
tcpip_handler(void)
{
    static uint8_t *appdata=NULL;
    short int code=0;
    int j=0;
    uint8_t isme=0;
    //printf("size of %d %d %d \n ",sizeof(int),sizeof(short int), sizeof(char));
    if(uip_newdata()) {

        appdata = (uint8_t *) uip_appdata;
        MAPPER_GET_PACKETDATA(code,appdata);
        switch(code)
        {
        case 1:
            add_node(appdata);
            break;
        case 2:
            //adding neighbor info and ranks
            j=update_info(appdata);
            //printf("updating node info by unicast \n");
            if(j==2)
            {
                printf("Duplicate packet returning\n");
                return;
            }
            if(etimer_expired(&nw_settle_timer) && attack_processing==0)
                //if nw settle timer expires then only chk for attack
            {   //currently no victims
                MAPPER_GET_PACKETDATA(node_to_v,appdata);
                process_start(&validatepc_process, NULL);
            }
            break;
        case 5:
            rssi_received(appdata);
            break;
        case 3://victim packet code+dest+collouge
	    printf("victim packet received\n");	
            if(last_target==0 && monitor==0) { //monitor==0 ){
                printf("I am Vtm or mntring for sus\n");
                MAPPER_GET_PACKETDATA(isme,appdata);
                MAPPER_GET_PACKETDATA(monitor_target,appdata);
                if(isme!=myip.u8[15] )
                {
                    printf("not 4 me\n");
                    if(rssi_stored==3)
                        return;
                    suspect_monitor(isme,monitor_target);

                    return;
                }
                process_start(&monitor_process, NULL);

            } else {
                printf("re received victim\n");
            }
            break;
        }
    }

    if(monitor && (monitor_target==UIP_IP_BUF->srcipaddr.u8[15]))
    {
        printf("RSSI %d\n",packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45);
        mrssi[rssi_stored]=packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
        rssi_stored++;
    }
    //uip_udp_packet_sendto(server_conn, "p", sizeof("p"), &UIP_IP_BUF->srcipaddr, UIP_HTONS(12345));
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
    nodes[0].x=x_loc;
    nodes[0].y=y_loc;
    uip_ip6addr(&nodes[0].ip, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
    nodes[0].parent_id=1;
    nodes[0].neighbors=uip_ds6_nbr_num();

    printf("Node %u, X %d Y %d, P %u, Nbrs %d ",nodes[0].id, nodes[0].x, nodes[0].y, nodes[0].parent_id,nodes[0].neighbors);

    uip_ds6_nbr_t *nbr=NULL;
    int i=0;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
        nodes[0].neighbor[i].id=nbr->ipaddr.u8[15];
        printf(" Nbr %d: %u",i,nodes[0].neighbor[i].id);
        i++;
    }
    printf("\n");
    total_nodes++;
}




PROCESS_THREAD(distance_process, ev, data)
{
    static int i,j;
    static double x,y,distance_x,distance_y;
    PROCESS_BEGIN();
    for(i=0; i<total_nodes; i++)
    {
        printf("Distance for %d ",nodes[i].id);
        for(j=0; j<total_nodes; j++)
        {
            if(i==j)
                distance[i][j]=0;
            else
            {
                distance_x = nodes[i].x-nodes[j].x;
                distance_y = nodes[i].y-nodes[j].y;
                x= (distance_x * distance_x);
                y= (distance_y * distance_y);
                distance[i][j]= sqrt( (double)x+(double)y );
            }
            printf(" %d ",distance[i][j]);

        }
        printf("\n");
        PROCESS_PAUSE();
        //etimer_set(&wait_timer, CLOCK_SECOND*1);
        //PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));
    }
    PROCESS_EXIT();
    PROCESS_END();
}
PROCESS_THREAD(postrssi_process, ev, data)
{
    static int i=0,j=0,k=0,l=0,dist[15],distance1=0;
    static int attacker_found=0,attacker_count=0;
    static uint8_t attacker[15];
    static int	rank[15];
    PROCESS_BEGIN();
    printf("In postrssi \n");
    j=0;
    k=0;
    if(victim_count==0 && (victim[0].is_rssi_stored ==0 && victim[1].is_rssi_stored==0))
    {
        printf("No rssi val received \n");
        PROCESS_EXIT();
    }
    // calculating distance from rssi approximately
    for(i=0; i<victim_count+2; i++)
    {
        printf("Rssi %d : %d\n",i,victim[i].mrssi);
    }

    for(i=0; i<victim_count+2; i++)
    {
        distance1=0;
        k=0;
        for(j=0; j<100; j++) {

            if(rssi[j]==victim[i].mrssi ||(rssi[j]==victim[i].mrssi+1)||(rssi[j]==victim[i].mrssi-1))
            {
                distance1 +=j;
                k++;
            }

        }
        if(k!=0)// to avoid divide by zero
            dist[i]=distance1/k;
    }

    for(i=0; i<victim_count+2; i++)
    {
        printf("D %d : %d\n",i,dist[i]);
    }

    //getting node location in structure

    for(i=0; i<victim_count+2; i++) //points to victims
        for(j=0; j<total_nodes; j++) //points to node location in struct
        {
            if(victim[i].id==nodes[j].id)
            {
                for(l=0; l<total_nodes; l++) //points to attacker
                {
                    if(distance[j][l]==dist[i]||(distance[j][l]==dist[i]-1)||(distance[j][l]==dist[i]+1))
                    {
                        attacker_found=0;
                        printf("Attacker : %u by %u\n",nodes[l].id,nodes[j].id);
                        for(k=0; k<attacker_count; k++) //searching existing attacker
                        {
                            if(attacker[k]==nodes[l].id)
                            {
                                rank[k]=rank[k]+1;
                                attacker_found=1;
                                break;
                            }

                        }
                        if(attacker_found==0)
                        {
                            attacker[attacker_count]=nodes[l].id;
                            rank[attacker_count]=rank[attacker_count]+1;
                            attacker_count++;

                        }
                        if(attacker_count==0)
                        {
                            attacker[0]=nodes[l].id;
                            if(rank[0]==0)
                                rank[0]=1;
                            else
                                rank[0]=rank[0]+1;
                            attacker_count++;
                        }

                    }
                }
            }
        }

    //selecting suspect nodes form approximate distance
    float lm=0;	
    for(i=0; i<attacker_count; i++)
    {
        printf("Attacker %u probability %d/%d \n",attacker[i],rank[i],victim_count+2);
        attacker[i]=rank[i]=0;
    }
    printf("2 victims are %u %u\n",current_victim[0],current_victim[1]);
    attacker_count=0;
    victim_count=0;
    victim[0].is_rssi_stored = victim[1].is_rssi_stored=0;
    attack_processing=0;
    PROCESS_EXIT();
    PROCESS_END();
}

static void chk_my_nbr(void)
{
    nodes[0].neighbors=uip_ds6_nbr_num();

    printf("Node %u, Nbrs: %d ",nodes[0].id, nodes[0].neighbors);

    uip_ds6_nbr_t *nbr=NULL;
    int i=0;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {
        nodes[0].neighbor[i].id=nbr->ipaddr.u8[15];
        printf(" %u",nodes[0].neighbor[i].id);
        i++;
    }
    printf("\n");
}

PROCESS_THREAD(udp_server_process, ev, data)
{
    static int distance_flag,i;
    static struct etimer reset_timer,rssi_wait;
    static unsigned char rbuf[20], *buf_p;

    PROCESS_BEGIN();

    distance_flag=0;
    create_dag();

    server_conn = udp_new(NULL, NULL, NULL);
    if(server_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn, UIP_HTONS(2345));


    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
           UIP_HTONS(server_conn->rport));
    udp_bconn = udp_broadcast_new(UIP_HTONS(BROADCAST_PORT), tcpip_handler);

    init_rssi();
    uip_ip6addr(&myip, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
    PRINTF("Listen port: 2345, TTL=%u\n", server_conn->ttl);
    etimer_set(&nw_settle_timer, (nwsettime + 4*Nodes_in_nw-20)* CLOCK_SECOND);//network setting time
    etimer_set(&addmyinfo_wait, 30 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&addmyinfo_wait));
    addmyinfo();
    etimer_set(&reset_timer, (reset_time)*CLOCK_SECOND);
    


    while(1) {

        PROCESS_YIELD();
        if(rssi_stored==3 && monitor!=0)// if got 5 rssi value from parent or child
        {
            monitor=0;
            rssi_stored=0;

            buf_p=rbuf;
            MAPPER_ADD_PACKETDATA(buf_p,myip.u8[15]);
            MAPPER_ADD_PACKETDATA(buf_p,monitor_target);
            for(i=0; i<3; i++)
            {
                MAPPER_ADD_PACKETDATA(buf_p,mrssi[i]);
                printf("%d ",mrssi[i]);
            }
            printf("\n");
            if(current_victim[1]==myip.u8[15])
            {
                printf("Rssi stored\n");
                victim[1].id=myip.u8[15];
                victim[1].suspect=monitor_target;
                victim[1].mrssi=(int)(mrssi[0] + mrssi[1] + mrssi[2])/3;
                victim[1].is_rssi_stored=1;
                printf("snd rssi %d %u %u %d\n",5,myip.u8[15],monitor_target,victim[1].mrssi);
                monitor_target=0;
            }

        }
        if(ev==tcpip_event)
        {
            tcpip_handler();
        }

        if(rssi_timer_flag)
        {
            rssi_timer_flag=0;
            printf("Set rssi time using flag \n");
            etimer_set(&rssi_wait, rssi_wait_time*CLOCK_SECOND);
        }
        if(reset_timer_flag)
        {
            reset_timer_flag=0;
            printf("Reset time using flag \n");
            etimer_set(&reset_timer, reset_time*CLOCK_SECOND);
        }

        if(distance_flag==0 && etimer_expired(&nw_settle_timer))
        {        
            distance_flag=1;
            process_start(&distance_process, NULL);
        }
        if(rssi_flag==1 && etimer_expired(&rssi_wait))
        {
            rssi_flag=0;
            printf("Ready to process rssi\n");
            process_start(&postrssi_process, NULL);
        }

        if(etimer_expired(&reset_timer))
        {
            attack_processing=0;
            printf("Reset time expired %d %d\n",reset_time,rssi_wait_time);
            victim[0].id=victim[1].id=0;
            victim[0].is_rssi_stored=victim[1].is_rssi_stored=0;
            victim_count=0;
            current_victim[0]=0;
            current_victim[1]=0;

            monitor=0;
            monitor_target=0;
            last_target=0;
            suspect_c=0;
            suspect_send=0;

            etimer_set(&reset_timer, reset_time*CLOCK_SECOND);
            printf("Total nodes %d\n",total_nodes);
	    
	    chk_my_nbr();
	    node_to_v=1;
	    process_start(&validatepc_process, NULL);		
	    
            //for(i=0;i<total_nodes;i++)
            {
                //	printf("N %u x %d y %d p %u\n",nodes[i].id,nodes[i].x,nodes[i].y,nodes[i].parent_id);
            }
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
