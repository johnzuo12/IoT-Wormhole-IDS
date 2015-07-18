

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_



/* For Imin: Use 16 over NullRDC, 64 over Contiki MAC */
//#define ROLL_TM_CONF_IMIN_1         64

#define UIP_CONF_IPV6_RPL    1  
//#undef UIP_CONF_ND6_SEND_RA
//#define UIP_CONF_ROUTER    1
//#define UIP_ND6_SEND_RA 1
//#define UIP_CONF_ND6_SEND_RA         1
//#define DEBUG_LEVEL     1

//#undef UIP_CONF_ND6_SEND_NA
//#define UIP_CONF_ND6_SEND_NA 1 // exp added

//#define RPL_DAG_MC 0
//#define RPL_CONF_OF rpl_of0



#define UIP_CONF_TCP 0

//#define PROCESS_CONF_NO_PROCESS_NAMES   1

//#define QUEUEBUF_CONF_NUM         1
//#define QUEUEBUF_CONF_REF_NUM     0

//#define NETSTACK_CONF_MAC_SEQNO_HISTORY 2

//#define UIP_CONF_UDP_CONNS       2

//#define UIP_CONF_ROUTER                 1
//#define UIP_CONF_ND6_SEND_RA            0

//#define UIP_CONF_IPV6_RPL               1
//#define RPL_CONF_MAX_INSTANCES          1
//#define RPL_CONF_MAX_DAG_PER_INSTANCE   1

/* Code/RAM footprint savings so that things will fit on our device */
#undef UIP_CONF_DS6_NBR_NBU
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_NBR_NBU        12
#define UIP_CONF_DS6_ROUTE_NBU      12

#endif /* PROJECT_CONF_H_ */

//set always transmission range//interferance range to 0 best result

//for rank attack detection conpair with old rank srored from other neighbor entry if diff>256 min hop inc then attack

//CFLAGS += -ffunction-sections
//LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__


