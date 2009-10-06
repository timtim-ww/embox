/**
 * \file ip_v4.c
 *
 * \date 17.03.2009
 * \author sunnya
 */
#include "string.h"
#include "conio.h"
#include "common.h"
#include "net/net.h"
#include "inet/netinet/in.h"
#include "net/ip.h"
#include "net/inet_sock.h"
#include "net/if_ether.h"
#include "net/net_packet.h"
#include "net/net_device.h"

int ip_received_packet(net_packet *pack) {
	LOG_DEBUG("ip packet received\n");
	net_device_stats *stats = pack->netdev->get_stats(pack->netdev);
	iphdr *iph = pack->nh.iph;
	/**
	 *   RFC1122: 3.1.2.2 MUST silently discard any IP frame that fails the checksum.
	 *   Is the datagram acceptable?
	 *   1.  Length at least the size of an ip header
	 *   2.  Version of 4
	 *   3.  Checksums correctly. [Speed optimisation for later, skip loopback checksums]
	 *   4.  Doesn't have a bogus length
	 */
	if (iph->ihl < 5 || iph->version != 4) {
		LOG_ERROR("invalide IPv4 header\n");
		stats->rx_err += 1;
		return -1;
	}
	unsigned short tmp = iph->check;
	iph->check         = 0;
	if ( tmp != calc_checksumm(pack->nh.raw, IP_HEADER_SIZE)) {
		LOG_ERROR("bad ip checksum\n");
		stats->rx_crc_errors += 1;
		return -1;
	}

	unsigned int len = ntohs(iph->tot_len);
	if (pack->len < len || len < (iph->ihl*4)) {
		LOG_ERROR("invalide IPv4 header length\n");
		stats->rx_length_errors += 1;
		return -1;
	}
//	packet_dump(pack);

	if (ICMP_PROTO_TYPE == iph->proto) {
                icmp_rcv(pack);
        }
        if (UDP_PROTO_TYPE == iph->proto) {
                udp_rcv(pack);
        }
	return 0;
}

int rebuild_ip_header(net_packet *pack, unsigned char ttl, unsigned char proto,
			unsigned short id, unsigned short len, unsigned char saddr[4], unsigned char daddr[4]) {
	iphdr *hdr    = pack->nh.iph;
	hdr->version  = 4;
	hdr->ihl      = IP_HEADER_SIZE >> 2;
	memcpy(hdr->saddr, saddr, sizeof(hdr->saddr));
	memcpy(hdr->daddr, daddr, sizeof(hdr->daddr));
	hdr->tot_len  = len;
        hdr->ttl      = ttl;
        hdr->id       = id;
        hdr->tos      = 0;
        hdr->frag_off = IP_DF;
        hdr->proto    = proto;
        hdr->check    = 0;
        hdr->check    = calc_checksumm(pack->nh.raw, IP_HEADER_SIZE);
	return 0;
}

static int build_ip_packet(struct inet_sock *sk, net_packet *pack) {
	pack->nh.raw = pack->data + ETH_HEADER_SIZE;
	rebuild_ip_header(pack, sk->uc_ttl, sk->sk.sk_protocol,
			  sk->id, pack->len, sk->saddr, sk->daddr);
	return 0;
}

int ip_send_packet(struct inet_sock *sk, net_packet *pack) {
	LOG_DEBUG("ip_send_packet\n");
	build_ip_packet(sk, pack);
	pack->protocol = ETH_P_IP;
	pack->len += IP_HEADER_SIZE;
	return eth_send(pack);
}
