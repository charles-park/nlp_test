//------------------------------------------------------------------------------
//
// 2022.04.14 Network Label Printer Control(For ODROID JIG)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <ctype.h>
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

#define	PRINT_MAX_CHAR	18
#define	PRINT_MAX_LINE	3

/* Default port 8888 (ODROID NLP Server), zpl direct port 9100 */
static int NlpTCPPort = NET_DEFAULT_PORT;

//------------------------------------------------------------------------------
//	function prototype
//------------------------------------------------------------------------------
static int is_net_alive		(char *nlp_addr);
static int 	read_with_timeout	(int fd, char *buf, int buf_size, int timeout_ms);
static	void nlp_disconnect	(int nlp_fp);
static 	int nlp_connect 	(char *nlp_addr);
static 	int get_my_ip 		(char *nlp_addr);
static 	int ip_range_check 	(char *nlp_addr);
static	void convert_to_zpl (char *sbuf, char mtype, char *msg, char ch);
		void nlp_set_port	(int port);
		int nlp_status 		(char *nlp_addr);
		int nlp_find 		(char *nlp_addr);
		int nlp_write		(char *nlp_addr, char mtype, char *msg, char ch);
		int nlp_test 		(void);

//------------------------------------------------------------------------------
static int is_net_alive (char *nlp_addr)
{
	char buf[2048];
	FILE *fp;

	memset (buf, 0x00, sizeof(buf));
	sprintf (buf, "ping -c 1 -w 1 2<&1 %s", nlp_addr);
	fprintf(stdout, "%s [%s] = true\n", __func__, buf);
	if ((fp = popen(buf, "r")) != NULL) {
		memset (buf, 0x00, sizeof(buf));
		while (fgets(buf, 2048, fp)) {
			if (NULL != strstr(buf, "1 received")) {
				pclose(fp);
				fprintf(stdout, "%s = true\n", __func__);
				return 1;
			}
		}
		pclose(fp);
	}
	fprintf(stdout, "%s = false\n", __func__);
	return 0;
}

//------------------------------------------------------------------------------
// TCP/UDP 데이터 read (timeout가능)
//------------------------------------------------------------------------------
static int read_with_timeout(int fd, char *buf, int buf_size, int timeout_ms)
{
	int rx_len = 0;
	struct	timeval  timeout;
	fd_set	readFds;

	// recive time out config
	// Set 1ms timeout counter
	timeout.tv_sec  = 0;
	timeout.tv_usec = timeout_ms*1000;

	FD_ZERO(&readFds);
	FD_SET(fd, &readFds);
	select(fd+1, &readFds, NULL, NULL, &timeout);

	if(FD_ISSET(fd, &readFds))
	{
		rx_len = read(fd, buf, buf_size);
	}

	return rx_len;
}

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

	// ping test
	if (!is_net_alive(nlp_addr))
		return 0;

	if((nlp_fp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) <0){
		err("socket create error : \n");
		return 0;
	}
	len = sizeof(struct sockaddr_in);

	memset (&s_addr, 0, len);

	//소켓에 접속할 주소 지정
	s_addr.sin_family 		= AF_INET;
	s_addr.sin_addr.s_addr 	= inet_addr(nlp_addr);
	s_addr.sin_port 		= htons(NlpTCPPort);

	// 지정한 주소로 접속
	if(connect (nlp_fp, (struct sockaddr *)&s_addr, len) < 0) {
		err("connect error : %s\n", nlp_addr);
		nlp_disconnect (nlp_fp);
		return 0;
	}
	return nlp_fp;
}

//------------------------------------------------------------------------------
//
//  Direct 연결하여 프린트 할 수 있는 Format으로 데이터 변경 (ZD230D)
//
//	mtype : 0 (mac addr), 1 (error)
//	msg
//		mtype : 0 -> 00:1e:06:xx:xx:xx
//		mtype : 1 -> err1, err2, ...
//	ch : 0 (left), 1 (right)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void convert_to_zpl (char *sbuf, char mtype, char *msg, char ch)
{
	int len;
	if (mtype) {
		char err[20], *msg_ptr, err_len, line = 0;

		memset (err, 0x00, sizeof(err));
		len     = sprintf (&sbuf[0]  , "%s", "^XA");
		len    += sprintf (&sbuf[len], "%s", "^FO304,20");

		err_len = sprintf (&err[0], "%c ", ch ? '>' : '<');

		msg_ptr = strtok (msg, ",");
		while (msg_ptr != NULL) {
			if ((strlen(msg_ptr) + err_len) > PRINT_MAX_CHAR) {
				len += sprintf (&sbuf[len], "^FD%s^FS", err);
				memset (err, 0x00, sizeof(err));
				err_len = 0;
				if (line < PRINT_MAX_LINE-1) {
					line++;
				} else {
					len += sprintf (&sbuf[len], "%s", "^XZ");
					len += sprintf (&sbuf[len]  , "%s", "^XA");
					line = 0;
				}
				len += sprintf (&sbuf[len], "^FO304,%d", line * 20 + 20);
			}
			err_len += sprintf (&err[err_len], "%s ", msg_ptr);
			msg_ptr  = strtok (NULL, ",");
		}
		if (err_len) {
			len += sprintf (&sbuf[len], "^FD%s^FS", err);
			len += sprintf (&sbuf[len], "%s", "^XZ");
		}
	} else {
		char mac[18], *ptr;

		memset (mac, 0x00, sizeof(mac));
		len  = sprintf (&sbuf[0]  , "%s", "^XA");
		len += sprintf (&sbuf[len], "%s", "^CFC");
		len += sprintf (&sbuf[len], "%s", "^FO310,25");
		len += sprintf (&sbuf[len], "^FD%s^FS",
				ch == 0 ? "< forum.odroid.com" : "forum.odroid.com >");
		len += sprintf (&sbuf[len], "%s", "^FO316,55");

		sprintf(mac, "%s", "??:??:??:??:??:??");
		if ((ptr = strstr (msg, "001e06")) != NULL) {
			sprintf(mac, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
				toupper(ptr[0]), toupper(ptr[1]), toupper(ptr[2]), toupper(ptr[3]),
				toupper(ptr[4]), toupper(ptr[5]), toupper(ptr[6]), toupper(ptr[7]),
				toupper(ptr[8]), toupper(ptr[9]), toupper(ptr[10]), toupper(ptr[11]));
		}
		len += sprintf (&sbuf[len], "^FD%s^FS", mac);
		len += sprintf (&sbuf[len], "%s", "^XZ");
	}
	info ("msg : %s, %d\n", sbuf, len);
}

//------------------------------------------------------------------------------
//	ODROID-C4를 사용하여 프린트되어지는 포트는 TCP : 8888
//  Direct Net연결하여 프린트 되어지는 포트는 TCP : 9100 (Model ZD230D)
//------------------------------------------------------------------------------
void nlp_set_port (int port)
{
	NlpTCPPort = port;
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
//	프로그램 버전을 확인한다.
//
//	성공 return 1, 실패 return 0
//------------------------------------------------------------------------------
int nlp_version (char *nlp_addr, char *rdver)
{
	byte_t sbuf[16], rbuf[16];
	int nlp_fp = nlp_connect (nlp_addr), timeout = 1000;

	if ((nlp_fp > 0) && (NlpTCPPort == NET_DEFAULT_PORT)) {
		memset (sbuf, 0, sizeof(sbuf));
		memset (rbuf, 0, sizeof(rbuf));

		strncpy(sbuf, "version", strlen("version"));
		write (nlp_fp, sbuf, strlen(sbuf));
		// wait read ack
		if (read_with_timeout(nlp_fp, rbuf, sizeof(rbuf), timeout)) {
			info ("read version is %s\n", rbuf);
			strncpy(rdver, rbuf, strlen(rbuf));
		}	else
			info ("read time out %d ms or rbuf is NULL!\n", timeout);
	}
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
	sprintf(cmd, "nmap %s%d.* -p T:%4d --open", NET_IP_BASE, ip, NlpTCPPort);

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
//
//  TCP/IP port 8888로 응답이 있고 mac addr이 00:1e:06:xx:xx:xx인 경우 해당함.
//  기존의 Label Printer(GC420d)는 USB로 바로 연결되어야 하므로 Server역활인 ODROID-C4
//  에서 데이터를 받아 프린터에 전송하는 방식으로 동작시킴.
//
//	nmap 192.168.xxx.xxx -p T:8888 --open
//
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
	char sbuf[256], nlp_addr[20], nlp_ver[20];

	memset(nlp_ver , 0, sizeof(nlp_ver));
	memset(nlp_addr, 0, sizeof(nlp_addr));
	memcpy(nlp_addr, ip_addr, strlen(ip_addr));

	if (!nlp_status(nlp_addr)) {
		err("Netwrok Label Print not found!! (%s)\n", nlp_addr);
		return 0;
	}

	if (!(nlp_version (nlp_addr, nlp_ver))) {
		err("Network Label Printer get version error. ip = %s\n", nlp_addr);
		return 0;
	}

	if (!(nlp_fp = nlp_connect (nlp_addr))) {
		err("Network Label Printer connect error. ip = %s\n", nlp_addr);
		return 0;
	}

	// 받아온 문자열 합치기
	memset (sbuf, 0, sizeof(sbuf));

	if (NlpTCPPort == NET_DEFAULT_PORT) {
		if (!strncmp(nlp_ver, "202204", strlen("202204")-1)) {
			// charles modified version
			info ("new version nlp-printer. ver = %s\n", nlp_ver);
			sprintf(sbuf, "%s-%c,%s",
							ch    ? "right" : "left",
							mtype ?     'e' : 'm',
							msg);
		} else {
			info ("old version nlp-printer.\n");
			// original version
			if (mtype)
				sprintf(sbuf, "error,%c,%s", ch ? '>' : '<', msg);
			else
				sprintf(sbuf, "mac,%s", msg);
		}
	}
	else
		convert_to_zpl (sbuf, mtype, msg, ch);

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

