
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <linux/if_tun.h>
#include <linux/if_bridge.h>
#include <linux/if_packet.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <errno.h>
#include <net/route.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <error.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/route.h>
#include <sys/prctl.h>


#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/if_tun.h>
#define __USE_GNU

#include <sched.h>
#include <sys/sysinfo.h>

#include "log.h"
#include "utils.h"
#include "err.h"

/**
 * @brief 获取当前进程名
 * @param[in] data 用来存放获取到的进程名
 * @param[in] len data可用的长度
 * @return data
 */
char *get_curr_proc_name(char *data, int len)
{
    int32_t ret;
    int8_t *p;
    int8_t buf[1024] = { 0 };

	//获取当前进程的可执行文件的绝对路径，如/work/code/tets/a.out
    ret = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (ret < 0)
        return "NULL";

    buf[ret] = '\0';

    //如果 buf="/work/code/tets/a.out",通过下面的步骤得到"a.out"
    p = strrchr(buf, '/');
    if (p)
        snprintf(data, len, "%s", p + 1);
    else
        snprintf(data, len, "%s", buf);

    //then data=a.out
    return data;
}

void show_hex(uint8_t *data, int32_t len)
{
    int32_t i;

    for (i = 0; i < len; i++) {
		if (!(i % 16))
			printf("0x%08x	", i);

        printf("%02x ", data[i]);
		if (i % 16 == 15)
			printf("\n");
    }
    printf("\n\n");
}


uint16_t gen_checksum(const char *buf, int num_bytes)
{
    const uint16_t *half_words = (const uint16_t *)buf;
    unsigned sum = 0;
    for (int i = 0; i < num_bytes / 2; i++)
        sum += half_words[i];

    if (num_bytes & 1)
        sum += buf[num_bytes - 1];

    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum & 0xff0000) >> 16;
    sum = ~sum & 0xffff;

    return sum;
}

/**
 * @brief 初始化信号处理，将SIGPIPE、SIGUSR1、SIGUSR2忽略掉
 */
void init_server_signal()
{
    sigset_t sig;

    sigemptyset(&sig);
    sigaddset(&sig, SIGPIPE);
    sigaddset(&sig, SIGUSR1);
    sigaddset(&sig, SIGUSR2);
    sigprocmask(SIG_BLOCK, &sig, NULL);
}

/**
 * @brief 设置一个套接字的读取超时时间
 * @param[in] fd 套接字描述符
 * @param[in] ms 超时时间，单位毫秒
 */
int set_sock_timeout(int fd, int ms)
{
	struct timeval tv = {0, 0};

	tv.tv_usec = ms * 1000;

	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval));

    return RET_OK;
}

void replace_char(char *str, char src, char dst)
{
	while (*str) {
		if (*str == src)
			*str = dst;
		str++;
	}
}

int file_exist(const char *path)
{
	if (access(path, F_OK) == 0)
		return 1;
	else
		return 0;
}

int user_system(char *cmd)
{
	int ret;

	log_debug("run cmd:%s\n", cmd);
	ret = system(cmd);
	log_debug("system ret:%d\n", ret);

	return 0;
}

int user_system_with_ret(char *cmd)
{
	int ret;

	log_debug("run cmd:%s\n", cmd);
	ret = system(cmd);
	log_debug("system ret:%d\n", ret);

	return ret;
}

int dir_empty(char *dirname)
{
	struct dirent *ent;
    DIR *dir = NULL;
	int ret = 1;

	if (!dirname || !dirname[0])
		goto end;

	dir = opendir(dirname);

    if (!dir)
        goto end;

    while (ent = readdir (dir)) {
        if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name))
            continue;

        /*判断是否有目录和文件*/
        if ((ent->d_type == DT_DIR) || (ent->d_type == DT_REG)) {
	        ret = 0;
			goto end;
        }
    }
end:
	if (dir)
		closedir(dir);
    return ret;
}

char *get_string_from_cmd(char *cmd, char *buf, int len)
{
	FILE *pfd = NULL;

	pfd = popen(cmd, "r");
	if (!pfd)
		return NULL;

	if (!fgets(buf, len, pfd)) {
		pclose(pfd);
		return NULL;
	}
	pclose(pfd);
	return buf;
}

int get_num_from_cmd(char *cmd)
{
	char buf[128] = {0};

	if (!get_string_from_cmd(cmd, buf, sizeof(buf)))
		return 0;
	else
		return atoi(buf);

}

int vnet_alloc(int tun, char *name)
{
	struct ifreq ifr = {0};
	int fd, err;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		log_err("open /dev/net/tun fail.\n");
		return -1;
	}

	if (tun)
		ifr.ifr_flags = IFF_TUN;
	else
		ifr.ifr_flags = IFF_TAP;

	ifr.ifr_flags = ifr.ifr_flags | IFF_NO_PI;

	arr_strcpy(ifr.ifr_name, name);

	err = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (err < 0) {
		close(fd);
		log_err("ioctl TUNSETIFF fail.\n");
		return -1;
	}

	log_debug("Open %s device : %s sucess.\n", tun?"tun":"tap", ifr.ifr_name);

	return fd;
}

int set_if_flags(const char *interface_name, short flag)
{
    int s, ret;
    struct ifreq ifr = {0};

    //0表示不指定协议
    s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
        log_err("Error create socket :%d\n", errno);
        return -1;
    }

    strcpy(ifr.ifr_name, interface_name);

    ret = ioctl(s, SIOCGIFFLAGS, &ifr);
    if (ret < 0) {
        log_err("Error get flag %s :%d\n", interface_name, errno);
        close(s);
        return -1;
    }

    ifr.ifr_ifru.ifru_flags |= flag;

	ret = ioctl(s, SIOCSIFFLAGS, &ifr);
    if (ret < 0) {
        log_err("Error set flag %s :%d\n",interface_name, errno);
        close(s);
        return -1;
    }

    close(s);

    return 0;
}

int set_if_up(const char *ifname)
{
	return set_if_flags(ifname, IFF_UP);
}

int set_if_down(const char *ifname)
{
	return set_if_flags(ifname, (short) ~IFF_UP);
}

/**
 *  设置接口ip地址和掩码
 */
int set_ipaddr(char *interface_name, char *ip, char *netmask, int mtu)
{
    int s, ret = -1;
    struct ifreq ifr;
    struct sockaddr_in addr;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        log_err("open socket %s failed:%d\n",interface_name, errno);
        return -1;
    }

    strcpy(ifr.ifr_name, interface_name);

    bzero(&addr, sizeof(struct sockaddr_in));
    addr.sin_family = PF_INET;
    inet_aton(ip, &addr.sin_addr);

    memcpy(&ifr.ifr_ifru.ifru_addr, &addr, sizeof(struct sockaddr_in));

    ret = ioctl(s, SIOCSIFADDR, &ifr);
    if(ret < 0) {
        log_err("Error set %s ip :%d\n", interface_name, errno);
        close(s);
        return -1;
    }

    inet_pton(AF_INET, netmask, &addr.sin_addr);
    memcpy(&ifr.ifr_netmask, &addr, sizeof(addr));

    ret = ioctl(s, SIOCSIFNETMASK, (void *)&ifr);
    if (ret < 0) {
        perror("ioctl SIOCSIFNETMASK");
        close(s);
        return -1;
    }

	ifr.ifr_mtu = mtu;
	if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
		perror("ioctl SIOCSIFMTU");
		close(s);
		return -1;
	}

    close(s);
    return 0;
}

int bind_thread_to_cpu(int cpu, char *name)
{
	cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);

    CPU_SET(cpu, &cpu_set);


	prctl(PR_SET_NAME,name);

    return sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);
}


int udp_connect(char *ip, unsigned short port)
{
	int socketfd;
	struct sockaddr_in servaddr = {0};

	int ret;

	socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketfd < 0) {
		log_err("socket err.\n");
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	ret = connect(socketfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
	if (ret < 0) {
		log_err("connect err, %s\n", strerror(errno));
		close(socketfd);
		return -1;
	}

	return socketfd;
}


/*
int tcp_connect(char *ip, unsigned short port)
{
	int socketfd;
	struct sockaddr_in servaddr = {0};

	int ret;

	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketfd < 0) {
		log_err("socket err.\n");
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

    ret = connect(socketfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
	if (ret < 0) {
		log_err("connect err.\n");
		close(socketfd);
		return -1;
	}
	return socketfd;
}
*/


int udp_server_init(unsigned short port)
{
	int socketfd;
	struct sockaddr_in servaddr;
	int ret;

	socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketfd < 0) {
		log_err("socket err.\n");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   //本机所有ip

   	ret = bind(socketfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr));
	if(ret < 0) {
		log_err("bind err.\n");
		return -1;
	}

	return socketfd;
}

int tcp_server_init(unsigned short port)
{
    int enable = 1;
    struct sockaddr_in addr;

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        log_err("create socket failed!\n");
        return -1;
    }
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,
						(char *)&enable, sizeof (enable)) < 0) {
        log_err("set reuse option error!\n");
        goto err;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        log_err("bind error!\n");
        goto err;
    }
    if (listen(sfd, 5) < 0) {
        log_err("listen error!\n");
        goto err;
    }
    return sfd;
err:
    close(sfd);

    return -1;
}

int tcp_connect(char *ip, unsigned short port)
{
	int socketfd;
	struct sockaddr_in servaddr = {0};
	int on = 1;
	int ret;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);

	if (socketfd < 0) {
		log_err("socket err.\n");
		return -1;
	}

	ret = setsockopt(socketfd, IPPROTO_TCP, TCP_NODELAY,
				(unsigned char *)&on, sizeof(int));
	if (ret < 0) {
	    log_err("Close Nagle error.\n");
		return -1;
	}
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

    ret = connect(socketfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
	if (ret < 0) {
		log_err("connect err.\n");
		close(socketfd);
		return -1;
	}
	return socketfd;
}


// 功能：设置arp表项
// 参数：网卡名; IP地址和MAC地址对; arp表项标志位
// 返回：-1-设置失败, 0-设置成功
int set_arp_entry(const char *dev, char *ip, unsigned char mac[6])
{
    int                ret = -1;
    struct arpreq      arp_req;
    struct sockaddr_in *sin;
    int sfd;

    sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd < 0)
		return sfd;

    sin = (struct sockaddr_in *)&(arp_req.arp_pa);

    memset(&arp_req, 0, sizeof(arp_req));
    sin->sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(sin->sin_addr));
    strncpy(arp_req.arp_dev, dev, IFNAMSIZ-1);
    memcpy(arp_req.arp_ha.sa_data, mac, 6);
    arp_req.arp_flags = ATF_PERM | ATF_COM;

    ret = ioctl(sfd, SIOCSARP, &arp_req);
    if (ret < 0)
        perror("Set ARP entry failed : %s\n");
    close(sfd);
    return ret;
}

int get_mac(char *dev, unsigned char dest_mac[6])
{
	struct ifreq ifreq = {0};
    int sock;

    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0) {
            perror("socket");
            return -1;
    }

    arr_strcpy(ifreq.ifr_name, dev);
    if(ioctl(sock, SIOCGIFHWADDR, &ifreq) < 0) {
            perror("ioctl");
            return -1;
    }
	memcpy(dest_mac, ifreq.ifr_hwaddr.sa_data, 6);

	close(sock);

    return 0;
}

int get_rand_range(int min, int max)
{
	/* 若min=10，max=100，rand() %91=（0~90），再加上10可得到10~100 */
	return rand() % (max -min + 1) + min;
}

