#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>

static int tun_assign(int fd, char *if_name);
static int tun_mac(int fd, char *if_name, uint8_t *src_mac);
static int tun_mtu(int fd, char *if_name, unsigned int mtu_frame);
static int tun_up(int fd, char *if_name);

struct tun_handle *tun_open(char *if_name, uint8_t *src_mac,
	unsigned int mtu_frame)
{
	struct tun_handle *tun;
	int fd, sock, ret;

	tun = malloc(sizeof(struct tun_handle));
	if(!tun)
		goto err_tun_alloc;

	tun->fd = open("/dev/net/tun", O_RDWR); 
	if(tun->fd < 0)
		goto err_tun_open;

	/* Must open a normal socket (UDP in this case) for some ioctl */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
		goto err_sock_open;

	ret = tun_assign(tun->fd, if_name);
	if(ret < 0)
		goto err_tun_assign;

	ret = tun_mac(tun->fd, if_name, src_mac);
	if(ret < 0)
		goto err_tun_mac;

	ret = tun_ifindex(sock, if_name);
	if(ret < 0)
		goto err_tun_ifindex;
	tun->ifindex = ret;

	ret = tun_mtu(sock, if_name, mtu_frame);
	if(ret < 0)
		goto err_tun_mtu;
	tun->mtu = mtu_frame;

	ret = tun_up(sock, if_name);
	if(ret < 0)
		goto err_tun_up;

	close(sock);
	return tun;

err_tun_up:
err_tun_mtu:
err_tun_mac:
err_tun_assign:
	close(sock);
err_sock_open:
	close(fd);
err_tun_open:
	free(tun);
err_tun_alloc:
	return NULL;
}

void tun_close(struct tun_handle *tun)
{
	close(tun->fd);
	free(tun);
	return;
}

static int tun_assign(int fd, char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	ret = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if(ret < 0){
		perror("tun assign");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_mac(int fd, char *if_name, uint8_t *src_mac)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	memcpy(ifr.ifr_hwaddr.sa_data, src_mac, ETH_ALEN);
	
	ret = ioctl(fd, SIOCSIFHWADDR, (void *)&ifr);
	if(ret < 0) {
		perror("tun mac");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_mtu(int fd, char *if_name, unsigned int mtu_frame)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_mtu = mtu_frame;

	ret = ioctl(fd, SIOCSIFMTU, (void *)&ifr);
	if(ret < 0) {
		perror("tun mtu");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_up(int fd, char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ifr.ifr_flags = IFF_UP;

	ret = ioctl(fd, SIOCSIFFLAGS, (void *)&ifr);
	if(ret < 0) {
		perror("tun up");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

static int tun_ifindex(int fd, char *if_name)
{
	struct ifreq ifr;
	int ret;

	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	ret = ioctl(fd, SIOCGIFINDEX, (void *)&ifr);
	if(ret < 0) {
		perror("tun ifindex");
		goto err_tun_ioctl;
	}

	return 0;

err_tun_ioctl:
	return -1;
}

struct tun_instance *tun_instance_alloc(struct tun_handle **th_list,
	int tun_num)
{
	struct tun_instance *instance;
	int i;

	instance = malloc(sizeof(struct tun_instance));
	if(!instance)
		goto err_instance_alloc;

	instance->ports = malloc(sizeof(struct tun_port) * tun_num);
	if(!instance->ports){
		printf("failed to allocate port for each instance\n");
		goto err_alloc_ports;
	}

	for(i = 0; i < ih_num; i++){
		instance->ports[i].fd = th_list[i]->fd;
		instance->ports[i].ifindex = th_list[i]->fd;
		instance->ports[i].mtu = th_list[i]->mtu_frame;
	}

	return instance;

err_alloc_ports:
	free(instance);
err_instance_alloc:
	return NULL;
}

void tun_instance_release(struct tun_instance *instance)
{
	free(instance->ports);
	free(instance);

	return;
}
