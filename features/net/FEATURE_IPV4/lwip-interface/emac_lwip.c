/* mbed Microcontroller Library
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "platform.h"

#if DEVICE_EMAC

#include "emac_api.h"
#include "emac_stack_mem.h"
#include "lwip/tcpip.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

static err_t emac_lwip_low_level_output(struct netif *netif, struct pbuf *p)
{
    emac_interface_t *mac = (emac_interface_t *)netif->state;
    bool ret = mac->ops.link_out(mac, (emac_stack_mem_t *)p);

    return ret ? ERR_OK : ERR_IF;
}

static err_t emac_lwip_output(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{
    /* Only send packets if the link is up */
    if (netif->flags & NETIF_FLAG_UP) {
        return etharp_output(netif, q, ipaddr);
    }

    return ERR_CONN;
}

static void emac_lwip_input(void *data, emac_stack_t *buf)
{
    struct eth_hdr *ethhdr;
    struct pbuf *p = (struct pbuf *)buf;
    struct netif *netif = (struct netif *)data;

    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) {
        LWIP_DEBUGF(NETIF_DEBUG, ("Emac LWIP: IP input error\n"));

        pbuf_free(p);
    }
}

static void emac_lwip_state_change(void *data, bool up)
{
    struct netif *netif = (struct netif *)data;

    if (up) {
        tcpip_callback_with_block((tcpip_callback_fn)netif_set_link_up, netif, 1);
    } else {
        tcpip_callback_with_block((tcpip_callback_fn)netif_set_link_down, netif, 1);
    }
}

err_t emac_lwip_if_init(struct netif *netif)
{
    int err = ERR_OK;
    emac_interface_t *mac = (emac_interface_t *)netif->state;

    mac->ops.set_link_input_cb(mac, emac_lwip_input, netif);
    mac->ops.set_link_state_cb(mac, emac_lwip_state_change, netif);

    netif->hwaddr_len = mac->ops.get_hwaddr_size(mac);
    mac->ops.get_hwaddr(mac, netif->hwaddr);

    netif->mtu = mac->ops.get_mtu_size(mac);

    /* Interface capabilities */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;

    mac->ops.get_ifname(mac, netif->name, 2);

    netif->output = emac_lwip_output;
    netif->linkoutput = emac_lwip_low_level_output;

    if (!mac->ops.power_up(mac)) {
        err = ERR_IF;
    }

    return err;
}

/* That's a compatibility layer for lwip_stack.c, it's already done by emac_interface in power_up and power_down */
void eth_arch_enable_interrupts(void)
{

}

void eth_arch_disable_interrupts(void)
{

}

#endif /* DEVICE_EMAC */
