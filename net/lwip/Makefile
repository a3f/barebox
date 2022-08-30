# SPDX-License-Identifier: GPL-2.0-only

KBUILD_CPPFLAGS += \
 -I$(srctree)/include/stdc \
 -I$(srctree)/net/lwip/contrib/ports/barebox/port/include \
 -I$(srctree)/net/lwip/contrib/ports/barebox/lib/ \
 -I$(srctree)/net/lwip/contrib \
 -I$(srctree)/net/lwip/src/include \

obj-$(CONFIG_LWIP_EXAMPLE) += contrib/examples/example_app/
obj-$(CONFIG_LWIP_EXAMPLE) += contrib/ports/barebox/example_app/

obj-$(CONFIG_LWIP_CORE) += contrib/ports/barebox/port/

obj-$(CONFIG_LWIP_CORE) += src/core/init.o
obj-$(CONFIG_LWIP_CORE) += src/core/def.o
obj-$(CONFIG_LWIP_CORE) += src/core/dns.o
obj-$(CONFIG_LWIP_CORE) += src/core/inet_chksum.o
obj-$(CONFIG_LWIP_CORE) += src/core/ip.o
obj-$(CONFIG_LWIP_CORE) += src/core/mem.o
obj-$(CONFIG_LWIP_CORE) += src/core/memp.o
obj-$(CONFIG_LWIP_CORE) += src/core/netif.o
obj-$(CONFIG_LWIP_CORE) += src/core/pbuf.o
obj-$(CONFIG_LWIP_CORE) += src/core/raw.o
obj-$(CONFIG_LWIP_CORE) += src/core/stats.o
obj-$(CONFIG_LWIP_CORE) += src/core/sys.o
obj-$(CONFIG_LWIP_CORE) += src/core/altcp.o
obj-$(CONFIG_LWIP_CORE) += src/core/altcp_alloc.o
obj-$(CONFIG_LWIP_CORE) += src/core/altcp_tcp.o
obj-$(CONFIG_LWIP_CORE) += src/core/tcp.o
obj-$(CONFIG_LWIP_CORE) += src/core/tcp_in.o
obj-$(CONFIG_LWIP_CORE) += src/core/tcp_out.o
obj-$(CONFIG_LWIP_CORE) += src/core/timeouts.o
obj-$(CONFIG_LWIP_CORE) += src/core/udp.o

obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/acd.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/autoip.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/dhcp.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/etharp.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/icmp.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/igmp.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/ip4_frag.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/ip4.o
obj-$(CONFIG_LWIP_CORE4) += src/core/ipv4/ip4_addr.o

obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/dhcp6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/ethip6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/icmp6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/inet6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/ip6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/ip6_addr.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/ip6_frag.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/mld6.o
obj-$(CONFIG_LWIP_CORE6) += src/core/ipv6/nd6.o

# APIFILES: The files which implement the sequential and socket APIs.
obj-$(CONFIG_LWIP_API) += src/api/api_lib.o
obj-$(CONFIG_LWIP_API) += src/api/api_msg.o
obj-$(CONFIG_LWIP_API) += src/api/err.o
obj-$(CONFIG_LWIP_API) += src/api/if_api.o
obj-$(CONFIG_LWIP_API) += src/api/netbuf.o
obj-$(CONFIG_LWIP_API) += src/api/netdb.o
obj-$(CONFIG_LWIP_API) += src/api/netifapi.o
obj-$(CONFIG_LWIP_API) += src/api/sockets.o
obj-$(CONFIG_LWIP_API) += src/api/tcpip.o

# Files implementing various generic network interface functions
obj-$(CONFIG_LWIP_NETIF) += src/api/tcpip.o
obj-$(CONFIG_LWIP_NETIF) += src/netif/ethernet.o
obj-$(CONFIG_LWIP_NETIF) += src/netif/bridgeif.o
obj-$(CONFIG_LWIP_NETIF) += src/netif/bridgeif_fdb.o
obj-$(CONFIG_LWIP_NETIF) += src/netif/slipif.o

# 6LoWPAN
obj-$(CONFIG_LWIP_6LOWPAN) += src/netif/lowpan6_common.o
obj-$(CONFIG_LWIP_6LOWPAN) += src/netif/lowpan6.o
obj-$(CONFIG_LWIP_6LOWPAN) += src/netif/lowpan6_ble.o
obj-$(CONFIG_LWIP_6LOWPAN) += src/netif/zepif.o

# PPP
obj-$(CONFIG_LWIP_PPP) += src/netif/zepif.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/auth.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/ccp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/chap-md5.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/chap_ms.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/chap-new.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/demand.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/eap.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/ecp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/eui64.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/fsm.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/ipcp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/ipv6cp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/lcp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/magic.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/mppe.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/multilink.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/ppp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/pppapi.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/pppcrypt.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/pppoe.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/pppol2tp.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/pppos.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/upap.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/utils.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/vj.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/polarssl/arc4.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/polarssl/des.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/polarssl/md4.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/polarssl/md5.o
obj-$(CONFIG_LWIP_PPP) += src/netif/ppp/polarssl/sha1.o

# SNMPv3 agent
obj-$(CONFIG_LWIP_SNMP) += src/netif/ppp/polarssl/sha1.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_asn1.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_core.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_icmp.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_interfaces.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_ip.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_snmp.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_system.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_tcp.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_mib2_udp.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_snmpv2_framework.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_snmpv2_usm.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_msg.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmpv3.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_netconn.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_pbuf_stream.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_raw.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_scalar.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_table.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_threadsync.o
obj-$(CONFIG_LWIP_SNMP) += src/apps/snmp/snmp_traps.o

# HTTP server + client
obj-$(CONFIG_LWIP_HTTP) += src/apps/http/altcp_proxyconnect.o
obj-$(CONFIG_LWIP_HTTP) += src/apps/http/fs.o
obj-$(CONFIG_LWIP_HTTP) += src/apps/http/http_client.o
obj-$(CONFIG_LWIP_HTTP) += src/apps/http/httpd.o

# MAKEFSDATA HTTP server host utility
hostprogs-$(CONFIG_LWIP_MAKEFSDATA) += src/apps/http/makefsdata/makefsdata.o

# IPERF server
obj-$(CONFIG_LWIP_IPERF) += src/apps/lwiperf/lwiperf.o

# SMTP client
obj-$(CONFIG_LWIP_SMTP) += src/apps/smtp/smtp.o

# SNTP client
obj-$(CONFIG_LWIP_SNTP) += src/apps/sntp/sntp.o

# MDNS responder
obj-$(CONFIG_LWIP_MDNS) += src/apps/mdns/mdns.o
obj-$(CONFIG_LWIP_MDNS) += src/apps/mdns/mdns_out.o
obj-$(CONFIG_LWIP_MDNS) += src/apps/mdns/mdns_domain.o

# NetBIOS name server
obj-$(CONFIG_LWIP_NETBIOS) += src/apps/netbiosns/netbiosns.o

# TFTP server files
obj-$(CONFIG_LWIP_TFTP) += src/apps/tftp/tftp.o

# MQTT client files
obj-$(CONFIG_LWIP_MQTT) += src/apps/mqtt/mqtt.o

# ARM MBEDTLS related files of lwIP rep
obj-$(CONFIG_LWIP_MBEDTLS) += src/apps/altcp_tls/altcp_tls_mbedtls.o
obj-$(CONFIG_LWIP_MBEDTLS) += src/apps/altcp_tls/altcp_tls_mbedtls_mem.o
obj-$(CONFIG_LWIP_MBEDTLS) += src/apps/snmp/snmpv3_mbedtls.o
