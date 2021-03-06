/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "fal_nat.h"
#include "fal_ip.h"
#include "hsl_api.h"
#include "hsl.h"

#ifdef ISISC
#include "isisc_acl.h"
#include "isisc_reg.h"
#else
#include "isis_acl.h"
#include "isis_reg.h"
#endif

#define MAX_PPPOE_PASSTHROUGH_NUM 4
/* ACL list priority */
#define S17_ACL_LIST_PRIO_HIGH  0
#define S17_ACL_LIST_PRIO_MID   1
#define S17_ACL_LIST_PRIO_LOW   2
#define S17_ACL_LIST_PRIO_LOWEST    3

#define S17_ACL_LIST_IPCONF 0
#define S17_ACL_LIST_UDP0   1
#define S17_ACL_LIST_DROUTE 2
#define S17_ACL_LIST_PPPOE  3
#define S17_ACL_LIST_IPV6MLD    5
#define S17_ACL_LIST_IPV6_SOLICITED_NODE    7
#define S17_ACL_LIST_IPV6_NODEINFO_QUERY    9
#define S17_ACL_LIST_IPV6_SEXTUPLE0_GROUP   11
#define S17_ACL_LIST_IPV6_QUINTRUPLE0_1_GROUP   13
#define S17_ACL_LIST_UPNP_SSDP  15
#define S17_ACL_LIST_PPPOE_PASSTHROUGH_LAN_TO_WAN 17
#define S17_ACL_LIST_PPPOE_PASSTHROUGH_WAN_TO_LAN 19
#define S17_ACL_LIST_PLC_FILTER 22
#define S17_ACL_LIST_REDIRECT_INTERNAL_IP_TO_CPU_ON_WAN 26
#define S17_ACL_ICMP_FROM_WAN 27

#ifdef AP152
#define S17_WAN_PORT    1
#define S17_CPU_PORT    0

#define S17_LAN_PORT0   2
#define S17_LAN_PORT1   3
#define S17_LAN_PORT2   4
#define S17_LAN_PORT3   5
#else
/* port # of WAN, 1 for DB120 demo board */
#define S17_WAN_PORT    5
#define S17_CPU_PORT    0

#define S17_LAN_PORT0   1
#define S17_LAN_PORT1   2
#define S17_LAN_PORT2   3
#define S17_LAN_PORT3   4
#endif

extern void athrs17_reg_write(unsigned int reg_addr, unsigned int reg_val);
extern unsigned int athrs17_reg_read(unsigned int reg_addr);

uint32_t get_aclrulemask(void);
void set_aclrulemask(uint32_t acl_list);

void droute_add_acl_rules(uint32_t local_ip, uint32_t gw_entry_id);
void pppoe_add_acl_rules(uint32_t wan_ip, uint32_t local_ip, uint32_t gw_entry_id);
void ip_conflict_add_acl_rules(uint32_t wan_ip, uint32_t lan_ip, uint32_t gw_entry_id);
void ipv6_snooping_solicted_node_add_acl_rules(void);
void ipv6_snooping_nodeinfo_query_add_acl_rules(void);
void ipv6_snooping_sextuple0_group_add_acl_rules(void);
void ipv6_snooping_quintruple0_1_group_add_acl_rules ( void );
void upnp_ssdp_add_acl_rules(void);
unsigned int isis_set_pppoe_passthrough(int enable);
unsigned int isis_enable_pppoe_discovery_acl(void);
int pppoe_passthrough_acl_rules(uint32_t gw_entry_id, unsigned char *mac);
