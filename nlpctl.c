//------------------------------------------------------------------------------
//
// 2022.04.14 Network Label Printer Control(For ODROID JIG)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ioctl.h> 

#include "typedefs.h"

//------------------------------------------------------------------------------
#define	NET_DEFAULT_NAME	"eth0"
#define	NET_DEFAULT_PORT	8888
#define NET_IP_BASE			"192.168."
#define TIMEOUT_SEC			10

//------------------------------------------------------------------------------
//	function prototype
//------------------------------------------------------------------------------
static void nlp_disconnect 	(int nlp_fp);
static 	int nlp_connect 	(char *nlp_addr);
static 	int get_my_ip 		(char *nlp_addr);
static 	int ip_range_check 	(char *nlp_addr);
		int nlp_status 		(char *nlp_addr);
		int nlp_find 		(char *nlp_addr);
		int nlp_write		(char *nlp_addr, char mtype, char *msg, char ch);
		int nlp_test 		(void);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//	연결을 해제한다.
//------------------------------------------------------------------------------
static void nlp_disconnect (int nlp_fp)
{
	if (nlp_fp)
		close(nlp_fp);
}

//------------------------------------------------------------------------------
//	현재 device의 eth0에 할당되어진 ip를 얻어온다.
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
static int get_my_ip (char *nlp_addr)
{
	int fd;
	struct ifreq ifr;
	unsigned char ipstr[16];

	/* this entire function is almost copied from ethtool source code */
	/* Open control socket. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		err("Cannot get control socket\n");
		return 0;
	}
	strncpy(ifr.ifr_name, NET_DEFAULT_NAME, IFNAMSIZ); 
	if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) { 
		fprintf(stdout, "SIOCGIFADDR ioctl Error!!\n"); 
		close(fd);
		return 0;
	}
	memset(ipstr, 0x00, sizeof(ipstr));
	inet_ntop(AF_INET, ifr.ifr_addr.sa_data+2, ipstr, sizeof(struct sockaddr)); 
	strncpy (nlp_addr, ipstr, strlen(ipstr));

	info("My own ip addr(NET_NAME = %s) = %s\n", NET_DEFAULT_NAME, ipstr);
	return 1;
}

//------------------------------------------------------------------------------
//	주소 범위가 현재 장치의 주소 영역에 있는지 확인한다.
//
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
static int ip_range_check (char *nlp_addr)
{
	char *m_tok, *n_tok, my_ip[20], nlp_ip[20];
	int m1, m2, m3, n1, n2, n3;

	memset(my_ip, 0, sizeof(my_ip));	memset(nlp_ip, 0, sizeof(nlp_ip));

	if (!get_my_ip(my_ip)) {
		err ("Network device not found!\n");
		return 0;
	}

	strncpy(nlp_ip, nlp_addr, strlen(nlp_addr));

	m_tok = strtok(my_ip, ".");		m1 = atoi(m_tok);
	m_tok = strtok( NULL, ".");		m2 = atoi(m_tok);
	m_tok = strtok( NULL, ".");		m3 = atoi(m_tok);

	n_tok = strtok(nlp_ip, ".");	n1 = atoi(n_tok);
	n_tok = strtok(  NULL, ".");	n2 = atoi(n_tok);
	n_tok = strtok(  NULL, ".");	n3 = atoi(n_tok);

	if ((m1 != n1) || (m2 != n2) || (m3 != n3))
		return 0;

	return 1;
}

//------------------------------------------------------------------------------
//	입력된 주소로 연결한다.
//
//	성공 return nlp_fp, 실패 return 0
//------------------------------------------------------------------------------
static int nlp_connect (char *nlp_addr)
{
	int nlp_fp, len;
	struct sockaddr_in s_addr;

	if (!ip_range_check (nlp_addr)) {
		err ("Out of range IP Address! (%s)\n", nlp_addr);
		return 0;
	}

	if((nlp_fp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <0){
		err("socket create error : \n");
		return 0;
	}
	len = sizeof(struct sockaddr_in);

	memset (&s_addr, 0, len);

	//소켓에 접속할 주소 지정
	s_addr.sin_family 		= AF_INET;
	s_addr.sin_addr.s_addr 	= inet_addr(nlp_addr);
	s_addr.sin_port 		= htons(NET_DEFAULT_PORT);

	// 지정한 주소로 접속
	if(connect (nlp_fp, (struct sockaddr *)&s_addr, len) < 0) {
		err("connect error : %s\n", nlp_addr);
		nlp_disconnect (nlp_fp);
		return 0;
	}
	return nlp_fp;
}

//------------------------------------------------------------------------------
//	입력되어진 주소로 연결을 시도한다.
//
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
int nlp_status (char *nlp_addr)
{
	int nlp_fp = nlp_connect (nlp_addr);

	nlp_disconnect(nlp_fp);

	return nlp_fp ? 1 : 0;
}

//------------------------------------------------------------------------------
//	nmap을 실행하여 나의 eth0에 할당되어진 현재 주소를 얻은 후 
//	같은 네트워크상의 연결된 장치를 스켄하여 연결을 시도한다.
//	성공시 입력 변수에 접속되어진 주소를 복사한다.
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
int nlp_find (char *nlp_addr)
{
	FILE *fp;
	char cmd[80], rbuff[4096], *ip_tok;
	int ip;

	if (!get_my_ip(nlp_addr))	{
		err ("Network device not found!\n");
		return 0;
	}

	ip_tok = strtok(nlp_addr, ".");	ip_tok = strtok(NULL,     ".");
	ip_tok = strtok(NULL,     ".");	ip = atoi(ip_tok);

	memset(cmd, 0, sizeof(cmd));
	sprintf(cmd, "nmap -sP %s%d.*", NET_IP_BASE, ip);

	if (NULL == (fp = popen(cmd, "r")))
	{
		err("popen() error!\n");
		return 0;
	}

	while (fgets(rbuff, 4096, fp)) {
		ip_tok = strstr(rbuff, NET_IP_BASE);
		if (ip_tok != NULL) {
			if (nlp_status (ip_tok)) {
				strncpy(nlp_addr, ip_tok, strlen(ip_tok)-1);
				pclose(fp);
				return 1;
			}
		}
	}
	pclose(fp);
	return 0;
}

//------------------------------------------------------------------------------
//	Network label printer에 MAC 또는 에러메세지를 출력한다.
//  입력된 주소를 가지고 connect 상태를 확인한다.
//	실패시 현재 자신의 주소를 확인 후 자신의 주소를 기준으로 nmap을 실행한다.
//	nmap -sP 192.168.xxx.xxx
//	scan되어진 ip중 연결되는 ip가 있다면 프린터라고 간주한다.
//	프린터에 메세지를 출력하고 출력이 정상인지 확인한다.
//	정상적으로 연결되어진 ip를 입력되어진 주소영역에 저장한다.
//
//	mtype : 0 (mac addr), 1 (error)
//	msg
//		mtype : 0 -> 00:1e:06:xx:xx:xx
//		mtype : 1 -> err1, err2, ...
//	ch : 0 (left), 1 (right)
//
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
int nlp_write (char *ip_addr, char mtype, char *msg, char ch)
{
	int nlp_fp, len;
	struct sockaddr_in s_addr;
	char sbuf[64], nlp_addr[20];

	memset(nlp_addr, 0, sizeof(nlp_addr));
	memcpy(nlp_addr, ip_addr, strlen(ip_addr));

	if (!nlp_status(nlp_addr)) {
		if(!nlp_find (nlp_addr)) {
			err("Netwrok Label Print not found!! (%s)\n", nlp_addr);
			return 0;
		}
		strncpy(ip_addr, nlp_addr, strlen(nlp_addr));
		info ("Network Label Printer found. IP address : %s\n", ip_addr);
	}

	if (!(nlp_fp = nlp_connect (nlp_addr))) {
		err("Network Label Printer connect error. ip = %s\n", nlp_addr);
		return 0;
	}

	// 받아온 문자열 합치기
	memset (sbuf, 0, sizeof(sbuf));
	#if 0 
		// original version
		if (mtype)
			sprintf(sbuf, "error,%s", msg);
		else
			sprintf(sbuf, "mac,%s", msg);
	#else
		// charles modified version
		sprintf(sbuf, "%s-%c,%s",
						ch    ? "right" : "left",
						mtype ?     'e' : 'm',
						msg);
	#endif

	// 받아온 문자열 전송
	if ((len = write (nlp_fp, sbuf, strlen(sbuf))) != strlen(sbuf))
		err ("send bytes error : buf = %ld, write = %d\n", strlen(sbuf), len);

	// 소켓 닫음
	nlp_disconnect (nlp_fp);

	return 1;
}

//------------------------------------------------------------------------------
int nlp_test (void)
{
	char ipaddr[20];
	memset(ipaddr, 0, sizeof(ipaddr));

	sprintf(ipaddr, "192.168.0.10");

	nlp_write (ipaddr, 0, "001e06123456", 0);
	nlp_write (ipaddr, 0, "001e06123456", 1);
	nlp_write (ipaddr, 1, "sata,usb3,usb2,usb1", 0);
	nlp_write (ipaddr, 1, "sata,usb3,usb2,usb1", 1);

	return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
