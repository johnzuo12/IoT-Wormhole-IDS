#include "contiki.h"

#include <stddef.h> /* For offsetof */
#include <stdio.h>
#include "contiki-conf.h"



#include "net/netstack.h"


#include "cc2420.h"
#include "cc2420_const.h"





static signed char rss;
  static signed char rss_val;
  static signed char rss_offset;
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  PROCESS_BEGIN();

  printf(" Hello, world\n ");
while(1){

  rss_val = cc2420_last_rssi;
  rss_offset=-45;
  rss=rss_val + rss_offset;
  printf("RSSI of Last Packet Received is rssi cc2420_last_rssi cc2420_rssi() %d %d %d\n",rss,cc2420_last_rssi, cc2420_rssi());
}
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
