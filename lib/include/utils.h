#ifndef _UTILS_H_
#define _UTILS_H_


#include <linux/if_ether.h>
#include <stdint.h>
#include <linux/ip.h>



#define arr_cnt(array) (sizeof(array) / sizeof(typeof(*array)))

#define arr_strcpy(dst,src) \
do { \
	strncpy(dst, src, sizeof(dst) -1); \
	dst[sizeof(dst) -1] = 0; \
}while(0)

void replace_char(char *str, char src, char dst);

int file_exist(const char *path);

int user_system(char *cmd);

int user_system_with_ret(char *cmd);

char *get_curr_proc_name(char *data, int len);

void show_hex(unsigned char *data, int len);

#define VNET_TYPE_TAP 0
#define VNET_TYPE_TUN 1

int vnet_alloc(int tun, char *name);
int bind_thread_to_cpu(int cpu, char *name);
int set_ipaddr(char *interface_name, char *ip, char *netmask, int mtu);
int set_if_up(const char *ifname);
int set_if_down(const char *ifname);
int set_if_flags(const char *interface_name, short flag);

static inline struct ethhdr *user_ethhdr(unsigned char *data)
{
    return (struct ethhdr *)data;
}

static inline struct iphdr *user_iphdr(unsigned char *data)
{
    //以太网帧头部 ethhdr，6位源地址，6位目的地址，2位类型
    //IP帧头部 iphdr, 20位内容
    return (struct iphdr *)(data + sizeof(struct ethhdr));
}

static inline struct udphdr *user_udphdr(unsigned char *data)
{
    //ip头的ihl表示ip的长度，单位为4，所以ilh*4
    return (struct udphdr *)(data + sizeof(struct ethhdr) + user_iphdr(data)->ihl * 4);
}

int get_num_from_cmd(char *cmd);

char *get_string_from_cmd(char *cmd, char *buf, int len);

int udp_connect(char *ip, unsigned short port);

int udp_server_init(unsigned short port);
int set_arp_entry(const char *dev, char *ip, unsigned char mac[6]);

int get_mac(char *dev, unsigned char dest_mac[6]);
int tcp_server_init(unsigned short port);
int tcp_connect(char *ip, unsigned short port);
int get_rand_range(int min, int max);
uint16_t gen_checksum(const char *buf, int num_bytes);

#endif
