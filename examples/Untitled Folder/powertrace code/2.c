#include "node-id.h"
#include "ids-common.h"
#include "net/mac/sicslowmac.c"
#include "dev/button-sensor.h"
static struct uip_udp_conn *udp_bconn;
static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr, myip;
extern rpl_instance_t instance_table[];

static uint8_t nbrs[10],last_target=0,suspect_c=0,suspect_send=0, orig_nbrs[Neighbor_queue],orig_num_nbrs=0;
static int msg_instance=0,monitor=0,reset_time=reset_tym,send_time=send_tym,nwsettime=nwsettym;
static int way=1,rssi_stored=0,reset_timer_flag=0,send_timer_flag=0,attack_time=attack_tym;
static uint16_t nbr_change=0;
uint8_t monitor_target=0;
static signed char mrssi[3];

static unsigned char nbuf[30],fbuf[30]; //do change in prepare forward pack

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UP");
PROCESS(sendpacket_process, "Spp");
PROCESS(sendrssi_process, "rsp");
PROCESS(sendfpacket_process, "sfpp");
PROCESS(monitor_process, "mp");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

static void
send_broadcast(unsigned char *buf, int size)
{
    //printf("In send B: \n");
    uip_create_linklocal_allnodes_mcast(&udp_bconn->ripaddr);
    uip_udp_packet_send(udp_bconn, buf, size);
    uip_create_unspecified(&udp_bconn->ripaddr);
}

PROCESS_THREAD(monitor_process, ev, data)
{
    static uint8_t a;
    static int n;
    static unsigned char *sbuf_p, sbuf[10];
    static struct etimer wait_timer;
    static int code=3;	//victim code
    PROCESS_BEGIN();

    a=monitor_target;

    if(a==0)
        PROCESS_EXIT();

    sbuf_p = sbuf;//code+dest+collouge
    MAPPER_ADD_PACKETDATA(sbuf_p,code);
    MAPPER_ADD_PACKETDATA(sbuf_p,a);
    MAPPER_ADD_PACKETDATA(sbuf_p,myip.u8[15]); // sending to parent its child node and it is victim


    if(last_target==a)
    {
        //printf("last mntr %u same ret\n",last_target);
        PROCESS_EXIT();
    }

    //printf("Monitoring for %u last target %u \n",monitor_target,last_target);
    monitor=1;


    last_target=a;

    reset_timer_flag = send_timer_flag=1;

    for(n=0; n<5; n++) {

        send_broadcast(sbuf,sizeof(sbuf));

        etimer_set(&wait_timer, CLOCK_SECOND*2);
        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));

        //printf("packet sent to %u last_target %u\n",monitor_target,last_target);

    }
    //printf("Monitor process exit\n");
    PROCESS_EXIT();
    PROCESS_END();
}


void forward_rssi(uint8_t *appdata)
{
    unsigned char buf[20], *buf_p;
    buf_p=buf;
    int code=5;

    reset_timer_flag = send_timer_flag=1;

    MAPPER_ADD_PACKETDATA(buf_p,code);

    memcpy(buf_p, appdata, 18);
    //printf("RSSI frwrd %d\n",sizeof(buf));
    uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
    uip_create_unspecified(&client_conn->ripaddr);
}

void forward_victimmsg(uint8_t *appdata)
{
    unsigned char buf[10], *buf_p;
    buf_p=buf;
    uint8_t a;
    int code=3;
    uip_ipaddr_t tempadd;
    MAPPER_ADD_PACKETDATA(buf_p,code);	
    memcpy(buf_p, appdata, 6);
    MAPPER_GET_PACKETDATA(a,appdata);
    //printf("Victim msg frwrd %d\n",sizeof(buf));

        tempadd.u16[0]=0x80fe;
        tempadd.u16[1]=0;
        tempadd.u16[2]=0;
        tempadd.u16[3]=0;
        tempadd.u16[4]=0x1202;
        tempadd.u8[10]=0x74;
        tempadd.u8[11]=a;
        tempadd.u8[12]=0;
        tempadd.u8[13]=a;
        tempadd.u8[14]=a;
        tempadd.u8[15]=a;
    
    uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &tempadd, UIP_HTONS(10000+(int)a));
    uip_create_unspecified(&client_conn->ripaddr);
}

void suspect_monitor(uint8_t a,uint8_t b)
{
    int i=0,j=0,k=0;
    //printf("In suspect monitor\n");

    reset_timer_flag = send_timer_flag=1;

    for(i=0; i<orig_num_nbrs; i++) {

        if(orig_nbrs[i]==a)
            j=1;
        if(orig_nbrs[i]==b)
            k=1;
    }
    if(k==0 && j==0)
    {
        //printf("monitoring for unknown\n");
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
          //  printf("monitoring for %u\n",a);
            return;
        }

        if(k==0 && j==1) {
            monitor_target=b;
            last_target=b;
            monitor=1;
            rssi_stored=0;
            //printf("monitoring for %u\n",b);
            return;
        }
    }
}

static void prepare_forward_packet(uint8_t *appdata)
{
    uint8_t nodeid;
    int code=2;

    reset_timer_flag = send_timer_flag=1;

    // printf("Forw by B\n");
    MAPPER_GET_PACKETDATA(nodeid,appdata);

    //printf("In B recv Code %d osndr %u myid %u \n", code, nodeid, myip.u8[15]);

    //printf("Data size %d \n",uip_datalen());
    unsigned char *buf_p;
    buf_p=fbuf;
    MAPPER_ADD_PACKETDATA(buf_p,code); //1
    MAPPER_ADD_PACKETDATA(buf_p, myip.u8[15]);//1 sender
    MAPPER_ADD_PACKETDATA(buf_p,nodeid);//1 info owner

    memcpy(buf_p, appdata, 30 - 3);		//hardcoded may be change if size of nbuf and fbuf changes

    //printf("B D U buf %d \n", sizeof(fbuf));
    //printf("received a broadcast of %d bytes: %s\n", uip_datalen(),uip_appdata);
}

PROCESS_THREAD(sendfpacket_process, ev, data)
{
    PROCESS_BEGIN();

    uip_udp_packet_sendto(client_conn, fbuf, sizeof(fbuf), &server_ipaddr, UIP_HTONS(2345));
    uip_create_unspecified(&client_conn->ripaddr);
    //printf("B D U buf %d \n", sizeof(fbuf));

    PROCESS_EXIT();
    PROCESS_END();
}

static void
tcpip_handler(void)
{
    static uint8_t *appdata=NULL;
    int code=0;
    uint8_t isme=0;
    if(uip_newdata()) {

        appdata = (uint8_t *) uip_appdata;
        MAPPER_GET_PACKETDATA(code,appdata);
        switch(code)
        {
        case 2://adding neighbor info and ranks
            prepare_forward_packet(appdata);
            process_start(&sendfpacket_process, NULL);
            break;
        case 3://victim packet code+dest+collouge
            if(last_target==0 && monitor==0) { //monitor==0 ){
               // printf("I am Vtm or mntring for sus\n");
                MAPPER_GET_PACKETDATA(isme,appdata);
                MAPPER_GET_PACKETDATA(monitor_target,appdata);
                if(isme!=myip.u8[15])
                {
                //    printf("not 4 me\n");
                    if(rssi_stored==3)
                        return;
                    suspect_monitor(isme,monitor_target);
                    return;
                }
		
                process_start(&monitor_process, NULL);

            } else {
              //  printf("re received victim\n");
            }
            break;
	case 4://forwarding victim form root node
		forward_victimmsg(appdata);
		break;
        case 5://Send rssi packet
            forward_rssi(appdata);
            break;
        }
    }

    if(monitor && (monitor_target==UIP_IP_BUF->srcipaddr.u8[15]))
    {
        //printf("RSSI %d\n",packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45);
        mrssi[rssi_stored]=packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
        rssi_stored++;
    }
}

int prepare_packet(int mode)
{
    uip_ds6_nbr_t *nbr=NULL;
    int num = uip_ds6_nbr_num();
    // code + node + id_of_intermediate + msg_instance + own rank + parent + rank + numof_nbr*(nbr+rank)
    unsigned char *buf_p=nbuf;

    int i=0;
    num = uip_ds6_nbr_num();

    if(!parent_ipaddr)// if parent is null return
    {
       // printf("Parent is 0 ret\n");
        //return 0;
    }

    i=2;

    MAPPER_ADD_PACKETDATA(buf_p, i);
    MAPPER_ADD_PACKETDATA(buf_p, myip.u8[15]);//sender	
    MAPPER_ADD_PACKETDATA(buf_p, myip.u8[15]);//info owner

    if(mode==1)		//for first time setting
    {   msg_instance = 100*myip.u8[15];
        //num_nbrs=num;
        orig_num_nbrs=0;
    }
    else
    {
        if(num>orig_num_nbrs)
        {   nbr_change=1;
           // printf("set nbr chng\n");
        }
    }

    MAPPER_ADD_PACKETDATA(buf_p, msg_instance);        //added to track limited boradcasting
    msg_instance++;

    MAPPER_ADD_PACKETDATA(buf_p, parent_ipaddr);
    MAPPER_ADD_PACKETDATA(buf_p, num);

    //printf("%02x P %u Buf %d m_inst %d No of nbrs %d : ",myip.u8[15],parent_ipaddr, sizeof(nbuf), msg_instance-1,num);

    int l=0;
    for(nbr = nbr_table_head(ds6_neighbors); nbr != NULL; nbr = nbr_table_next(ds6_neighbors, nbr)) {

        MAPPER_ADD_PACKETDATA(buf_p,nbr->ipaddr.u8[15]);
	//printf(" %u ",nbr->ipaddr.u8[15]);
        nbrs[l]=nbr->ipaddr.u8[15];
        if(mode==1)
        {
            orig_nbrs[orig_num_nbrs++]=nbr->ipaddr.u8[15];
        }

    }
    //printf("\n");
    int x=node_loc_x, y=node_loc_y;
    MAPPER_ADD_PACKETDATA(buf_p,x);
    MAPPER_ADD_PACKETDATA(buf_p,y);
    return 1;
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sendpacket_process, ev, data)
{
    static struct etimer wait_timer;
    PROCESS_BEGIN();

    if(way)
    {
        uip_udp_packet_sendto(client_conn, nbuf, sizeof(nbuf), &server_ipaddr, UIP_HTONS(2345));
        uip_create_unspecified(&client_conn->ripaddr);
        nbr_change=0;
        //printf("Way =1 U\n");
    }
    else
    {
        send_broadcast(nbuf,sizeof(nbuf));

        etimer_set(&wait_timer, CLOCK_SECOND*2);
        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));

        //printf("Way =0 B \n");
        uip_udp_packet_sendto(client_conn, nbuf, sizeof(nbuf), &server_ipaddr, UIP_HTONS(2345));//temporaryly aadded
        uip_create_unspecified(&client_conn->ripaddr);
    }
    PROCESS_EXIT();
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/


static void
print_local_addresses(void)
{
    static int i;
    static uint8_t state;

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
    //printf("%u X %d Y %d Buf %d \n",myip.u8[15],x,y, sizeof_buf);

    uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));
    uip_create_unspecified(&client_conn->ripaddr);
}


PROCESS_THREAD(sendrssi_process, ev, data)
{
    static unsigned char rbuf[20], *buf_p;
    static int code = 5;
    static int i=0;
    static uint8_t a;
    static struct etimer wait_timer;
    PROCESS_BEGIN();

    buf_p=rbuf;
    MAPPER_ADD_PACKETDATA(buf_p,code);
    MAPPER_ADD_PACKETDATA(buf_p,myip.u8[15]);
    MAPPER_ADD_PACKETDATA(buf_p,monitor_target);
    //printf("snd rssi %d %u %u\n",code,myip.u8[15],monitor_target);
    for(i=0; i<3; i++)
    {
        MAPPER_ADD_PACKETDATA(buf_p,mrssi[i]);
        printf("%d ",mrssi[i]);
    }
    printf("\n");

    etimer_set(&wait_timer, CLOCK_SECOND*14);

    PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));

    for(i=0; i<2; i++) {

        etimer_set(&wait_timer, CLOCK_SECOND*(myip.u8[15]));

        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));
	
        uip_udp_packet_sendto(client_conn, rbuf, sizeof(rbuf), &server_ipaddr, UIP_HTONS(2345));
        uip_create_unspecified(&client_conn->ripaddr);
	
        etimer_set(&wait_timer, CLOCK_SECOND*(myip.u8[15]));
        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));
	
        uip_ipaddr_t tempadd;
        if(last_target)
            a=last_target;
        else
            a=parent_ipaddr;

        tempadd.u16[0]=0x80fe;
        tempadd.u16[1]=0;
        tempadd.u16[2]=0;
        tempadd.u16[3]=0;
        tempadd.u16[4]=0x1202;
        tempadd.u8[10]=0x74;
        tempadd.u8[11]=a;
        tempadd.u8[12]=0;
        tempadd.u8[13]=a;
        tempadd.u8[14]=a;
        tempadd.u8[15]=a;

        uip_udp_packet_sendto(client_conn, rbuf, sizeof(rbuf), &tempadd, UIP_HTONS(10000+(int)tempadd.u8[15]));
        uip_create_unspecified(&client_conn->ripaddr);

        etimer_set(&wait_timer, CLOCK_SECOND*(myip.u8[15]));
        PROCESS_WAIT_UNTIL(etimer_expired(&wait_timer));
	
        send_broadcast(rbuf,sizeof(rbuf));

    }
    //printf("snd rssi %d %u %u %d\n",code,myip.u8[15],monitor_target,sizeof(rbuf));
    monitor_target=0;
    monitor=0;
    rssi_stored=0;
    PROCESS_EXIT();
    PROCESS_END();

}

PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer reset_timer, start_timer, send_timer, attack_timer;
    static int flag=1, i_am_attacker=0;
    //static struct mt_thread  attackinit_thread;	//sending_thread,

    PROCESS_BEGIN();
    SENSORS_ACTIVATE(button_sensor);
    set_global_address();
	powertrace_start(CLOCK_SECOND * 60);
    //printf("UDP client process started\n");

    print_local_addresses();

    myip=uip_ds6_get_link_local(ADDR_PREFERRED)->ipaddr;

    /* new connection with remote host */
    client_conn = udp_new(NULL, NULL, NULL);
    if(client_conn == NULL) {
     //   printf("NUC E\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(10000+(int)myip.u8[15]));

    udp_bconn = udp_broadcast_new(UIP_HTONS(BROADCAST_PORT),tcpip_handler);
    if(udp_bconn == NULL) {
    //    printf("NUC E\n");
        PROCESS_EXIT();
    }
    //client_conn->ttl=200;
    //udp_bconn->ttl=200;
   // printf("ports %u/%u %d\n", UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport),client_conn->ttl);

    etimer_set(&start_timer, nwsettime * CLOCK_SECOND);//network setting time
    etimer_set(&send_timer, CLOCK_SECOND * (nwsettime + myip.u8[15] + 4*Nodes_in_network + send_time +10));
    //node check/send parent info
    unsigned char buf[3]="hi";
    etimer_set(&reset_timer, reset_time * CLOCK_SECOND);
    etimer_set(&attack_timer, attack_time * 60 * CLOCK_SECOND);	
    if(myip.u8[15]==attacker1)	
	i_am_attacker=1;
    if(myip.u8[15]==attacker2)	
	i_am_attacker=2;	
    if(myip.u8[15]==attacker3)	
	i_am_attacker=3;
    if(myip.u8[15]==attacker4)	
	i_am_attacker=4;
    	
    while(1) {

        PROCESS_YIELD();

        if(rssi_stored==3)// if got 5 rssi value from parent or child
        {
            monitor=0;
            rssi_stored=0;
            process_start(&sendrssi_process, NULL);
            etimer_set(&send_timer, CLOCK_SECOND*(reset_time + send_time + 2*myip.u8[15]));
        }
        if(ev==tcpip_event && attack_flag==0)
        {
            //printf("Tcpip\n");
            tcpip_handler();
        }
        if((ev==sensors_event) && (data == &button_sensor)) {
            if(attack_flag)
            {
           //     printf("Attack deactivated\n");
                attack_flag=0;
                attacker=0;
                dis_output(NULL);
            } else {

            //    printf("\nPacket Relay Attack activated\n");
                attack_flag=1;
                attacker=4;
		setreg(CC2420_MDMCTRL0,0);
                dis_output(NULL);
            }
        }
	if(etimer_expired(&attack_timer) && attack_flag==0 && i_am_attacker!=0)
        {
		attack_flag=1;
                attacker=i_am_attacker;
		setreg(CC2420_MDMCTRL0,0);
		printf("\nAttack activated attacker type %d\n",i_am_attacker);
	}
        if(send_timer_flag)
        {
            send_timer_flag=0;
            //printf("reset send time using flag \n");
            etimer_set(&send_timer, send_time*CLOCK_SECOND);
        }
        if(reset_timer_flag)
        {
            reset_timer_flag=0;
            //printf("Reset time using flag \n");
            etimer_set(&reset_timer, reset_time*CLOCK_SECOND);
        }
        if(etimer_expired(&send_timer) && attack_flag==0)
        {
            //etimer_set(&send_timer, CLOCK_SECOND*(send_time+Nodes_in_network+3*myip.u8[15]));
            etimer_set(&send_timer, CLOCK_SECOND*(send_time+ (random_rand() % send_time)));
            if(nbr_change==1 && monitor==0)        // send only at parent change event
            {

                if(prepare_packet(0))
                {
                    way=0;
                    process_start(&sendpacket_process, NULL);
                    //send nbr info by broadcast 0 for broadcast
                }

            }
            else if(monitor==0)
            {   //send_broadcast("hi",sizeof("hi"));
                prepare_packet(2);
                way=1;
                process_start(&sendpacket_process, NULL);
            }
        }
        if(etimer_expired(&start_timer) && flag==1)
        {
            flag=0;
            send_broadcast(buf,sizeof(buf));

            //etimer_set(&start_timer, CLOCK_SECOND*3);
            //PROCESS_WAIT_UNTIL(etimer_expired(&start_timer));

            //uip_udp_packet_sendto(client_conn, buf, sizeof(buf), &server_ipaddr, UIP_HTONS(2345));//temporaryly aadded
            //uip_create_unspecified(&client_conn->ripaddr);

	    etimer_set(&start_timer, CLOCK_SECOND*(myip.u8[15]));
	    PROCESS_WAIT_UNTIL(etimer_expired(&start_timer));	

            send_xy();

            etimer_set(&start_timer, CLOCK_SECOND*(Nodes_in_network*4));
            PROCESS_WAIT_UNTIL(etimer_expired(&start_timer));

            prepare_packet(1);
            way=1;
            process_start(&sendpacket_process, NULL);

        }
        if(etimer_expired(&reset_timer)) {
            //printf("Reset time expired\n");
            monitor=0;
            monitor_target=0;
            last_target=0;
            suspect_c=0;
            suspect_send=0;
	    	
            etimer_set(&reset_timer, CLOCK_SECOND*reset_time);
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
