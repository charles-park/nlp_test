//------------------------------------------------------------------------------
//
// 2022.04.14 Argument parser app. (chalres-park)
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <getopt.h>

#include "typedefs.h"
#include "nlpctl.h"
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
const char *OPT_NLP_ADDR = "192.168.0.0";
const char *OPT_MSG_STR = NULL;
static uchar_t OPT_CHANNEL = 0;
static bool OPT_NLP_FIND = false;
static bool OPT_MSG_ERR = false;

static uchar_t NlpIPAddr[20];
static uchar_t NlpMsg[64];

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage(const char *prog)
{
	printf("Usage: %s [-factm]\n", prog);
	puts("  -f --find_nlp  find network printer.\n"
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
			{ "nlp_addr",	1, 0, 'a' },
			{ "channle",	1, 0, 'c' },
			{ "msg_type",	1, 0, 't' },
			{ "msg_str",	1, 0, 'm' },
			{ NULL, 0, 0, 0 },
		};
		int c;

		c = getopt_long(argc, argv, "fa:c:t:m:", lopts, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			OPT_NLP_FIND = true;
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
		default:
			print_usage(argv[0]);
			break;
		}
	}
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
		info ("-m {string} option is missing.\n");
	}
//	nlp_test();
	return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
