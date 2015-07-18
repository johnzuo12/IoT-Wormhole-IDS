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
AUTOSTART_PROCESSES(&udp_client_process);
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
            //printf("\n");
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


PROCESS_THREAD(udp_client_process, ev, data)
{
    static struct etimer reset_timer, start_timer, send_timer, attack_timer;
    static int flag=1, i_am_attacker=0;
    //static struct mt_thread  attackinit_thread;	//sending_thread,

    PROCESS_BEGIN();
	powertrace_start(CLOCK_SECOND * 60);
    SENSORS_ACTIVATE(button_sensor);
    set_global_address();

    //printf("UDP client process started\n");

    print_local_addresses();

    myip=uip_ds6_get_link_local(ADDR_PREFERRED)->ipaddr;

    /* new connection with remote host */
    client_conn = udp_new(NULL, NULL, NULL);
    if(client_conn == NULL) {
        //printf("NUC E\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(10000+(int)myip.u8[15]));


    //client_conn->ttl=200;
    //udp_bconn->ttl=200;
    //printf("ports %u/%u %d\n", UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport),client_conn->ttl);

 

    	
    while(1) {

        PROCESS_YIELD();

       }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
