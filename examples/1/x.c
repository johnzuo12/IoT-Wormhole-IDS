  #include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"
#include "dev/watchdog.h"
#include "dev/leds.h"
#include "net/rpl/rpl.h"
#include "dev/button-sensor.h"
#include "debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

#define MAX_PAYLOAD_LEN 120
#define TABLE_LEN 5

void update_table(float tmpr, uint16_t lv);
void display_vtable();
static struct uip_udp_conn *server_conn;
static uint16_t buf[MAX_PAYLOAD_LEN];
static uint16_t len, lv,tv;
static numofcl = 0;
static float recvtemp;


// structure to hold ip and state
typedef struct {
    uip_ipaddr_t IP;
    uint8_t      nstatus;
    uint8_t      is_default;
    float        temp;
    uint16_t     light;
}verify;

static verify vtable[TABLE_LEN];

#define SERVER_REPLY          1

/* Should we act as RPL root? */
#define SERVER_RPL_ROOT       1

#if SERVER_RPL_ROOT
static uip_ipaddr_t ipaddr;
#endif
/*---------------------------------------------------------------------------*/
PROCESS(udp_server_process, "UDP server process");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  int i,x ;
  float g, sumtemp, avgtemp ; //For handling temperature float value
  memset(buf, 0, MAX_PAYLOAD_LEN); // set buffer
  if(uip_newdata()) {
     leds_on(LEDS_RED);
     len = uip_datalen();
     PRINTF("Incoming packet of length %d\r\n", len);
     memcpy(buf, uip_appdata, len);

     for (i=0; i<4; i++)
     {
         PRINTF("0x%04x ",buf[i]);
     }
     PRINTF("\n\r");
     g = (float)buf[1] / 100;
     recvtemp = buf[0] + g;
     lv = buf[2];
     tv = buf[3];



      printf("upper light value  = 0x%04x\n\r",  lv); //print light value in correct format
      printf("lower light value= 0x%04x\n\r", tv);
      printf("temperature value = %02f\n\r", recvtemp);

//    PRINTF("%u\n\r bytes from [", len);
//    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
//    PRINTF("]:%u\n", UIP_HTONS(UIP_UDP_BUF->srcport));

    update_table(recvtemp,lv);

    PRINTF("\n\rnumofcl = %d\n\r", numofcl);

// Table Full condition

    if ( numofcl == TABLE_LEN )
    {
        PRINTF("\n\rTable Full\n");

        //Calculate average
        sumtemp = 0;
        for(i = 0; i < TABLE_LEN; i++)
        {
            sumtemp += vtable[i].temp;
        }

        avgtemp = sumtemp / TABLE_LEN;
        PRINTF("\n\rAverage Temperature = %d\n", avgtemp);

        //clear sensor data ..temp,light,etc
        for (x=0; x<TABLE_LEN; x++)
            {
                //vtable[x].nstatus = 0;
                vtable[x].temp = 0;
                vtable[x].light = 0;
            }
    }

#if SERVER_REPLY
    uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    server_conn->rport = UIP_UDP_BUF->srcport;

    uip_udp_packet_send(server_conn, buf, len);
    /* Restore server connection to allow data from any node */
    uip_create_unspecified(&server_conn->ripaddr);
    server_conn->rport = 0;
#endif
  }
  leds_off(LEDS_RED);
  display_vtable();
  return;
}
/*---------------------------------------------------------------------------*/
#if (BUTTON_SENSOR_ON && (DEBUG==DEBUG_PRINT))
static void
print_stats()
{
  PRINTF("tl=%lu, ts=%lu, bs=%lu, bc=%lu\n",
         rimestats.toolong, rimestats.tooshort, rimestats.badsynch,
         rimestats.badcrc);
  PRINTF("llrx=%lu, lltx=%lu, rx=%lu, tx=%lu\n", rimestats.llrx,
         rimestats.lltx, rimestats.rx, rimestats.tx);
}
#endif
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state
        == ADDR_PREFERRED)) {
      PRINTF("  ");
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      if(state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------uip_newdata------------------------------------*/
#if SERVER_RPL_ROOT
void
create_dag()
{
  rpl_dag_t *dag;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);


  print_local_addresses();

  dag = rpl_set_root(RPL_DEFAULT_INSTANCE,
                     &uip_ds6_get_global(ADDR_PREFERRED)->ipaddr);
  if(dag != NULL) {
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("Created a new RPL dag with ID: ");
    PRINT6ADDR(&dag->dag_id);
    PRINTF("\n");
  }
}
#endif /* SERVER_RPL_ROOT */

void update_table(float tmpr, uint16_t lv)
{
    int x;
    uint8_t intbl = 0;
    uip_ipaddr_t defaultIP;
    uip_ipaddr(&defaultIP,0,0,0,0);
    for (x=0; x<TABLE_LEN; x++)
    {

        if(uip_ipaddr_cmp(&vtable[x].IP, &UIP_IP_BUF->srcipaddr)) //compare ip's / updating an existing client
        {
            //if (vtable[x].nstatus == 0)
            //{
                //vtable[x].nstatus = 1;
                vtable[x].temp = tmpr;
                vtable[x].light = lv;
            //}


            PRINTF("Existing field updated");
            break;
        }


        else if(vtable[x].is_default == 0)  // updating a null field/ adding new client
        {
            uip_ipaddr_copy(&vtable[x].IP, &UIP_IP_BUF->srcipaddr); //
            //vtable[x].nstatus = 1;
            vtable[x].is_default = 1;
            vtable[x].temp = tmpr;
            vtable[x].light = lv;
            PRINTF("new field updated");
            numofcl++;
            break;
        }
    }

}

void
display_vtable()
{
    int j;
     for(j = 0; j < TABLE_LEN; j++)
      {
         PRINTF("\n\rvtable[%d] --> uip_addr =",j);
         uip_debug_ipaddr_print(&vtable[j].IP);
         PRINTF( "status = %d\r\n", vtable[j].nstatus);
         PRINTF( "temperature = %02f\n", vtable[j].temp );
         PRINTF(" Light = 0x%04x\n\r", vtable[j].light);
         PRINTF( "is_default = %d\n\r", vtable[j].is_default);
      }

    }
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{

  uint8_t i;

  PROCESS_BEGIN();
  for(i = 0; i < TABLE_LEN; i++) //Initialize table by setting values to zero
  {
      uip_ipaddr(&vtable[i].IP,0,0,0,0);
      vtable[i].nstatus = 0;
      vtable[i].is_default = 0;
  }





#if SERVER_RPL_ROOT
  create_dag();
#endif

  server_conn = udp_new(NULL, UIP_HTONS(12345), NULL);
  udp_bind(server_conn, UIP_HTONS(2345));

  PRINTF("Listen port: 3000, TTL=%u\n", server_conn->ttl);

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {

      tcpip_handler();
#if (BUTTON_SENSOR_ON && (DEBUG==DEBUG_PRINT))
    } else if(ev == sensors_event && data == &button_sensor) {
      print_stats();
#endif /* BUTTON_SENSOR_ON */
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
