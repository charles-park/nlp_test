//------------------------------------------------------------------------------
//
// 2022.04.14 Network Label Printer Control(For ODROID JIG)
//
//------------------------------------------------------------------------------
//	function prototype
//------------------------------------------------------------------------------
extern	void nlp_set_port	(int port);
extern	int nlp_status 		(char *nlp_addr);
extern	int nlp_find 		(char *nlp_addr);
extern	int nlp_write		(char *nlp_addr, char mtype, char *msg, char ch);
extern	int nlp_test 		(void);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
