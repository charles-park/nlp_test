//------------------------------------------------------------------------------
//
// 2022.04.14 Argument parser app. (chalres-park)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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
#include "nlpctl.h"
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const char *OPT_NLP_ADDR = "192.168.0.0";
const char *OPT_MSG_STR = NULL;
static uchar_t OPT_CHANNEL = 0;
static bool OPT_NLP_FIND = false;
static bool OPT_MSG_ERR = false;
static bool OPT_MAC_PRINT = false;

static uchar_t NlpIPAddr[20];
static uchar_t NlpMsg[64];

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage(const char *prog)
{
	printf("Usage: %s [-fpactm]\n", prog);
	puts("  -f --find_nlp  find network printer ip.\n"
	     "  -z --zpl       zpl label printer direct connect (T:9100)\n"
		 "                 default npl_server connect (T:8888)\n"
	     "  -p --mac_print print device mac address.(find 00:1e:06:xx:xx:xx)\n"
	     "  -a --nlp_addr  Network printer ip address.(default = 192.168.0.0)\n"
	     "  -c --channel   Message channel (left or right, default = left)\n"
	     "  -t --msg_type  message type (mac or error, default = mac)\n"
	     "  -m --msg       print message.\n"
		 "                 type == mac, msg is 001e06??????\n"
	     "                 type == error, msg is usb,???,...\n"
		 "   e.g) nlp_test -a 192.168.20.10 -c left -t error -m usb,sata,hdd\n"
		 "        nlp_test -f -c right -t mac -m 001e06234567\n"
	);
	exit(1);
}

//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
	int i, c = strlen(p);

	for (i = 0; i < c; i++, p++)
		*p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
	int i, c = strlen(p);

	for (i = 0; i < c; i++, p++)
		*p = toupper(*p);
}

//------------------------------------------------------------------------------
static void parse_opts (int argc, char *argv[])
{
	while (1) {
		static const struct option lopts[] = {
			{ "find_nlp",  	0, 0, 'f' },
			{ "mac_print",	0, 0, 'p' },
			{ "nlp_addr",	1, 0, 'a' },
			{ "channle",	1, 0, 'c' },
			{ "msg_type",	1, 0, 't' },
			{ "msg_str",	1, 0, 'm' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "fpa:c:t:m:z", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			OPT_NLP_FIND = true;
			break;
		case 'p':
			OPT_MAC_PRINT = true;
			break;
		case 'a':
			OPT_NLP_ADDR = optarg;
			break;
		case 'c':
			tolowerstr (optarg);
            if (!strncmp("right", optarg, strlen("right")))
				OPT_CHANNEL = 1;
			else
				OPT_CHANNEL = 0;
			break;
		case 't':
			tolowerstr (optarg);
			if (!strncmp("error", optarg, strlen("error")))
				OPT_MSG_ERR = true;
			else
				OPT_MSG_ERR = false;
			break;
		case 'm':
			OPT_MSG_STR = optarg;
			break;
		case 'z':
			/* Connect directly to ZPL Porocol printers over a network.(ZD230D) */
			nlp_set_port (9100);
			break;
		default:
			print_usage(argv[0]);
			break;
		}
	}
}

//------------------------------------------------------------------------------
int get_mac_addr(byte_t *mac_str)
{
	int sock, if_count, i;
	struct ifconf ifc;
	struct ifreq ifr[10];
	unsigned char mac[6];

	memset(&ifc, 0, sizeof(struct ifconf));

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	// 검색 최대 개수는 10개
	ifc.ifc_len = 10 * sizeof(struct ifreq);
	ifc.ifc_buf = (char *)ifr;

	// 네트웨크 카드 장치의 정보 얻어옴.
	ioctl(sock, SIOCGIFCONF, (char *)&ifc);

	// 읽어온 장치의 개수 파악
	if_count = ifc.ifc_len / (sizeof(struct ifreq));
	for (i = 0; i < if_count; i++) {
		if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) == 0) {
			memcpy(mac, ifr[i].ifr_hwaddr.sa_data, 6);
			printf("find device (%s), mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
				ifr[i].ifr_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			memset (mac_str, 0, 20);
			sprintf(mac_str, "%02x%02x%02x%02x%02x%02x",
						mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
			if (!strncmp ("001e06", mac_str, strlen("001e06")))
				return 1;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    parse_opts(argc, argv);

	memset (NlpIPAddr, 0, sizeof(NlpIPAddr));
	strncpy (NlpIPAddr, OPT_NLP_ADDR, strlen(OPT_NLP_ADDR));

	if (OPT_NLP_FIND) {
		if (nlp_find (NlpIPAddr))
			info ("Network Label Printer Fond! IP = %s\n", NlpIPAddr);
		else {
			err ("Network Label Printer not found!\n");
			return 0;
		}
	}

	if (OPT_MAC_PRINT) {
		byte_t mac_str[20];
		if (get_mac_addr(mac_str)) {
			info ("Send to Net Printer(%s) %s!! (string : %s)\n",
					NlpIPAddr,
					nlp_write (NlpIPAddr, 0, mac_str, OPT_CHANNEL) ? "ok" : "false",
					mac_str);
		}
		return 0;
	}

	if (OPT_MSG_STR != NULL) {
		bool success = false;

		memset (NlpMsg, 0, sizeof(NlpMsg));
		strncpy (NlpMsg, OPT_MSG_STR, strlen(OPT_MSG_STR));
		success = nlp_write (NlpIPAddr, OPT_MSG_ERR, NlpMsg, OPT_CHANNEL);
		info ("Send to Net Printer(%s) %s!! (string : %s)\n",
				NlpIPAddr,
				success ? "ok" : "false",
				OPT_MSG_STR);
	} else {
		if (!OPT_MAC_PRINT && !OPT_NLP_FIND)
			info ("-m {string} option is missing.\n");
	}
//	nlp_test();
	return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
