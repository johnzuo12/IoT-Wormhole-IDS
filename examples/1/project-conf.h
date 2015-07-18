

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_



/* For Imin: Use 16 over NullRDC, 64 over Contiki MAC */
//#define ROLL_TM_CONF_IMIN_1         64

#undef UIP_CONF_IPV6_RPL      
#undef UIP_CONF_ND6_SEND_RA
#undef UIP_CONF_ROUTER    
#define UIP_CONF_ND6_SEND_RA         0
#define DEBUG_LEVEL     1

//#define RPL_CONF_OF rpl_of0

#define RPL_DAG_MC 0 /* Local identifier for empty MC */

#undef UIP_CONF_TCP
#define UIP_CONF_TCP 0

/* Code/RAM footprint savings so that things will fit on our device */
#undef UIP_CONF_DS6_NBR_NBU
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_NBR_NBU        10
#define UIP_CONF_DS6_ROUTE_NBU      10

#endif /* PROJECT_CONF_H_ */
