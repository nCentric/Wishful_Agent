/*
 * server.c
 *
 *  Created on: Jan 4, 2017
 *      Author: kvdp
 */

#define _GNU_SOURCE

#include "agent.h"

#include <stdio.h>
#include <string.h>
#include <zmq.h>
#include <unistd.h>
#include <stdlib.h>
#include "commands.h"
#include "Tools/Timer/Timer.h"


enum eTopic
{
	TOP_UNKNOWN,
	TOP_NODE_NEW,
	TOP_NODE_EXIT,
	TOP_CONTROLLER,
	TOP_MY_IP,
	TOP_BACKBONE,
	TOP_ALL,
	MAX_TOP
};

enum eCommand
{
	CMD_RX_UNKNOWN,
	CMD_TX_NODE_ADD,
	CMD_TX_NODE_DEL,
	CMD_TX_HELLO,
	CMD_TX_TC_DEV_ADD,
	CMD_TX_TC_DEV_DEL,
	CMD_TX_TC_FIL_ADD,
	CMD_TX_TC_FIL_DEL,
	CMD_RX_NODE_ACK,
	CMD_RX_HELLO,
	CMD_RX_INST_EGRESS,
	CMD_RX_SET_CHANNEL,
	CMD_RX_SET_TXPOWER,
	CMD_RX_SET_AIRFAIR,
	CMD_RX_GET_CHANMEAS,
	CMD_RX_GET_LISTRSSI,
	CMD_RX_MONITOR,
	CMD_RX_REBOOT,
	MAX_RX
};

enum AgentState
{
	ST_NOTINIT,
	ST_INIT,
	ST_CONNECT,
	ST_INTRODUCE,
	ST_PROCESS
};

/* Globals */
/* Global vars */
volatile int AllStop;

static void* zmq_context;
static void* zmq_sock_DL;
static void* zmq_sock_UL;
static int Connected;

static const char* tTopic[7] = { "UNKNOWN", "NEW_NODE", "NODE_EXIT", "Controller", "NODE_IP", "Backbone", "ALL"};
static const char* tCommand[18] = { "UNKNOWN", "NewNodeAdd", "NewNodeDel", "Hello", "DevAdd", "DevDel", "FilAdd", "FilDel", "NewNodeAck", "HelloMsg", "install_egress_scheduler", "set_channel", "set_txpower", "set_airfair", "get_chanmeas", "get_listrssi", "monitor_transmission_parameters", "reboot_sut"};

static const char* MyOwnIP;
enum AgentState State = ST_INIT;



/* Internal functions */
void my_free (void *data, void *hint)
{
    //  We've allocated the buffer using malloc and
    //  at this point we deallocate it using free.
    free (data);
}

static zmq_msg_t* CreateMsg(enum eTopic topic, enum eCommand cmd, void* data, int datasize)
{
	int Sz = 0;
	char* Work;
	void *hint = NULL;
	zmq_msg_t* Msg = NULL;
	const char* topicstr = NULL;

	/* Check params */
	if (topic == TOP_UNKNOWN || topic > MAX_TOP || cmd == CMD_RX_UNKNOWN || cmd > MAX_RX)
	{
		return NULL;
	}

	/* Create a new message */
	if (!(Msg = malloc(sizeof (zmq_msg_t))))
	{
		return NULL;
	}

	if (topic == TOP_MY_IP && MyOwnIP)
	{
		topicstr = MyOwnIP;
	}
	else
	{
		topicstr = tTopic[topic];
	}

	/* Calc needed memsize */
	Sz = strlen(topicstr);
	Sz += 1;	/* space */
	Sz += strlen(tCommand[cmd]);
	Sz += 1;	/* space */

	/* Create working mem */
	if (!(Work = (char*) calloc(Sz + datasize + 1, 1)))	/* +1 for 0-term safety */
	{
		free(Msg);
		return NULL;
	}

	/* Init */
	//zmq_msg_init (Msg);

	/* Add topic and Cmd */
	snprintf(Work, Sz + datasize + 1, "%s %s ", topicstr, tCommand[cmd]);


	/* Add optional data */
	if (data && datasize)
	{
		memcpy(&Work[Sz], data, datasize);
	}

	/* Add to message */
	zmq_msg_init_data (Msg, Work, Sz + datasize, my_free, hint);

	return Msg;
}

static int Agent_Init(const char* MyIP)
{
	int cores;
	int major;
	int minor;
	int patch;
	int linger = 100;
	int Timeout = 4000;

	if (!MyIP) return -1;

	/* CPU cores available */
	if ((cores = (int) sysconf(_SC_NPROCESSORS_ONLN)) <= 0)
	{
		printf("No CPU's available ..\n");
		return -1;
	}

	/* Limit cores used */
	if (cores > 2) cores = 2;

	zmq_version (&major, &minor, &patch);
	printf("ZMQ version: %d.%d.%d\n", major, minor, patch);
	printf("Cores used: %d\n", cores);

	/* Context */
	zmq_context = zmq_init(cores);

	/* Create/Configure Sockets */
	if (!(zmq_sock_DL = zmq_socket(zmq_context, ZMQ_SUB))		/* Downstream socket */
	 || zmq_setsockopt (zmq_sock_DL, ZMQ_SUBSCRIBE, "ALL", 3)	/* Listen only for messages directed to ALL */
	 || zmq_setsockopt (zmq_sock_DL, ZMQ_SUBSCRIBE, tTopic[TOP_NODE_NEW], strlen(tTopic[TOP_NODE_NEW]))	/* Listen only for messages directed to ME */
	 || zmq_setsockopt (zmq_sock_DL, ZMQ_SUBSCRIBE, MyIP, strlen(MyIP))	/* Listen only for messages directed to ME */
	 || zmq_setsockopt (zmq_sock_DL, ZMQ_RCVTIMEO, &Timeout, sizeof (Timeout))
	 || !(zmq_sock_UL = zmq_socket(zmq_context, ZMQ_PUB))		/* Upstream socket */
	 || zmq_setsockopt (zmq_sock_DL, ZMQ_LINGER, &linger, sizeof (linger)))	/* Max time in ms a message is allows to be send on socket closure before it is discarded */
	{
		/* Error - cleanup */
		Agent_Stop();
		printf("Error creating sockets\n");

		return -1;
	}

	return 0;
}

static int Agent_ReceiveMessage(char** Data)
{
	int RcvSz = 0;
	char* Rcv = NULL;
	int64_t more;
	size_t more_size = sizeof more;

	if (!Data)
	{
		return -1;
	}

	do
	{
		int Recv;
		zmq_msg_t part;

		more = 0;

		/* Init */
		zmq_msg_init (&part);

		/* Block until a message is available to be received from socket */
		Recv = zmq_recvmsg (zmq_sock_DL, &part, 0);
		if (Recv <= 0)
		{
			/* Error */
			zmq_msg_close (&part);
			if (Rcv) free (Rcv);

			return -1;
		}

		/* Make room for the data */
		Rcv = (char*) realloc(Rcv, RcvSz + Recv +2);	/* + Alloc an extra byte in the total array for 0-term */

		/* Data present? */
		if (zmq_msg_size(&part))
		{
			mempcpy(&Rcv[RcvSz], zmq_msg_data(&part), zmq_msg_size(&part));
			RcvSz += (int) zmq_msg_size(&part);
		}

		/* Add 0-term */
		Rcv[RcvSz] = 0;

		/* Determine if more message parts are to follow */
		zmq_getsockopt (zmq_sock_DL, ZMQ_RCVMORE, &more, &more_size);

		/* Cleanup */
		zmq_msg_close (&part);

		if (more)
		{
			Rcv[RcvSz] = ' ';
			Rcv[RcvSz + 1] = 0;
			RcvSz++;
		}

	} while (more);

	/* Provide to caller */
	*Data = Rcv;

	return RcvSz;
}

static int Agent_DecodeMessage(const char* msg, enum eTopic* Topic, enum eCommand* Command, char** data)
{
	char* MsgData = NULL;
	char a[32] = {0};
	char b[32] = {0};

	/* Param check */
	if (!msg || !Topic || !Command)
	{
		return -1;
	}

	/* Verify required data */
	if (sscanf(msg, "%s %s", a, b) == 2)
	{
		int len_a = strlen(a);
		int len_b = strlen(b);

		if (strlen(msg) > len_a + 1 + len_b + 1)
		{
			MsgData = (char*) &msg[len_a + 1 + len_b + 1];
		}

		/* Topic */
			 if (!strcasecmp(a, tTopic[TOP_NODE_NEW]))	*Topic = TOP_NODE_NEW;
		else if (!strcasecmp(a, tTopic[TOP_NODE_EXIT]))	*Topic = TOP_NODE_EXIT;
		else if (!strcasecmp(a, tTopic[TOP_CONTROLLER]))*Topic = TOP_CONTROLLER;
		else if (!strcasecmp(a, tTopic[TOP_BACKBONE]))	*Topic = TOP_BACKBONE;
		else if (!strcasecmp(a, tTopic[TOP_ALL]))		*Topic = TOP_ALL;
		else if (MyOwnIP && !strcasecmp(a, MyOwnIP))	*Topic = TOP_MY_IP;
		else 											*Topic = TOP_UNKNOWN;

		 /* Command */
			 if (!strcasecmp(b, tCommand[CMD_RX_NODE_ACK]))		*Command = CMD_RX_NODE_ACK;
		else if (!strcasecmp(b, tCommand[CMD_RX_HELLO]))		*Command = CMD_RX_HELLO;
		else if (!strcasecmp(b, tCommand[CMD_RX_INST_EGRESS]))	*Command = CMD_RX_INST_EGRESS;
		else if (!strcasecmp(b, tCommand[CMD_RX_SET_CHANNEL]))	*Command = CMD_RX_SET_CHANNEL;
		else if (!strcasecmp(b, tCommand[CMD_RX_SET_TXPOWER]))	*Command = CMD_RX_SET_TXPOWER;
		else if (!strcasecmp(b, tCommand[CMD_RX_SET_AIRFAIR]))	*Command = CMD_RX_SET_AIRFAIR;
		else if (!strcasecmp(b, tCommand[CMD_RX_GET_CHANMEAS]))	*Command = CMD_RX_GET_CHANMEAS;
		else if (!strcasecmp(b, tCommand[CMD_RX_GET_LISTRSSI]))	*Command = CMD_RX_GET_LISTRSSI;
		else if (!strcasecmp(b, tCommand[CMD_RX_MONITOR]))		*Command = CMD_RX_MONITOR;
		else if (!strcasecmp(b, tCommand[CMD_RX_REBOOT]))		*Command = CMD_RX_REBOOT;
		else 													*Command = CMD_RX_UNKNOWN;
	}

	/* Provide data to caller if requested */
	if (data)
	{
		*data = MsgData;
	}

	return 0;
}

static void Agent_Execute(enum eTopic Topic, enum eCommand Command, const char* data)
{
	static char Error[] = "Error";
	char* Reply = NULL;
	zmq_msg_t* Msg;

	/* Param check */
	if (!Topic || !Command)
	{
		return;
	}

	switch(Command)
	{
		case CMD_RX_NODE_ACK:
		{
			/* Connected to controller */
			Connected = 1;
			break;
		}

		case CMD_RX_HELLO:
		{
			break;
		}

		case CMD_RX_INST_EGRESS:
		{
			break;
		}

		case CMD_RX_SET_CHANNEL:
		{
			cmd_SetChannel(data, &Reply);
			break;
		}

		case CMD_RX_SET_TXPOWER:
		{
			cmd_SetTxpower(data, &Reply);
			break;
		}

		case CMD_RX_SET_AIRFAIR:
		{
			cmd_SetFairness(data, &Reply);
			break;
		}

		case CMD_RX_GET_CHANMEAS:
		{
			cmd_Getchanmeas(&Reply);
			break;
		}

		case CMD_RX_GET_LISTRSSI:
		{
			cmd_Getlistrssi(&Reply);
			break;
		}

		case CMD_RX_MONITOR:
		{
			break;
		}

		case CMD_RX_REBOOT:
		{
			cmd_Reboot();
			break;
		}

		default:;
	}

	/* Create ZMQ msg */
	if ((Msg = CreateMsg(Topic, Command, (Reply) ? Reply : Error, (Reply) ? strlen(Reply) : strlen(Error))))
	{
		/* Send on success */
		zmq_sendmsg(zmq_sock_UL, Msg, 0);

		/* Cleanup */
		zmq_msg_close(Msg);
	}

	if (Reply) free(Reply);
}

static int Agent_Introduce(const char* MyIP)
{
	int stat = -1;
	zmq_msg_t* Msg;

	/* Create draft msg */
	if (!(Msg = CreateMsg(TOP_NODE_NEW, CMD_TX_NODE_ADD, (void*) MyIP, strlen(MyIP))))
	{
		/* Could not create the msg */
		return -1;
	}

	/* Send */
	if (zmq_sendmsg(zmq_sock_UL, Msg, 0) <= 0)
	{
		zmq_msg_close(Msg);	/* Send failed! Cleanup ourselves */
		return -1;
	}

	zmq_msg_close(Msg);

	/* Wait for ACK from controller */
	char* RcvMsg = NULL;
	enum eTopic Topic = TOP_UNKNOWN;
	enum eCommand Command = CMD_RX_UNKNOWN;

	if (Agent_ReceiveMessage(&RcvMsg) > 0						/* Receive */
	&& !Agent_DecodeMessage(RcvMsg, &Topic, &Command, NULL)		/* Decode */
	&& (Topic == TOP_NODE_NEW || Topic == TOP_MY_IP)			/* Topic OK ? */
	&& Command == CMD_RX_NODE_ACK)								/* Command OK ? */
	{
		stat = 0;
	}

	if (RcvMsg) free(RcvMsg);

	return stat;
}

static void Agent_Cleanup()
{
	if (zmq_sock_DL)
	{
		zmq_close(zmq_sock_DL);
		zmq_sock_DL = 0;
	}

	if (zmq_sock_UL)
	{
		zmq_close(zmq_sock_UL);
		zmq_sock_UL = 0;
	}

	if (zmq_context)
	{
		zmq_term(zmq_context);
		zmq_context = NULL;
	}
}

/* Exported functions */
int Agent_Start(const char* MyIP, const char* ServerDL, const char* ServerUL)
{
	if (!MyIP || !ServerDL || !ServerUL)
	{
		return -1;
	}

	MyOwnIP = MyIP;


	while (!AllStop)
	{
		switch (State)
		{
			case ST_INIT:
			{
				if (Agent_Init(MyIP))
				{
					usleep(1000 * 200);
				}
				else
				{
					State = ST_CONNECT;
				}

				break;
			}

			case ST_CONNECT:
			{
				if (zmq_connect(zmq_sock_DL, ServerDL) || zmq_connect(zmq_sock_UL, ServerUL))
				{
					Agent_Cleanup();
					State = ST_INIT;
					usleep(1000 * 1000);
				}
				else
				{
					State = ST_INTRODUCE;
					usleep(1000 * 1000);
				}

				break;
			}

			case ST_INTRODUCE:
			{
				if (Agent_Introduce(MyIP))
				{
					Agent_Cleanup();
					State = ST_INIT;
					usleep(1000 * 1000);
				}
				else
				{
					State = ST_PROCESS;
				}

				break;
			}

			case ST_PROCESS:
			{
				int Sz;
				char* Msg = NULL;
				enum eTopic Topic = TOP_UNKNOWN;
				enum eCommand Cmd = CMD_RX_UNKNOWN;
				char* Data = NULL;

				/* Decode */
				if ((Sz = Agent_ReceiveMessage(&Msg)) <= 0			/* Receive */
				|| Agent_DecodeMessage(Msg, &Topic, &Cmd, &Data))	/* Decode */
				{
					if (Msg) free(Msg);
					Agent_Cleanup();
					State = ST_INIT;
					usleep(1000 * 1000);
					break;
				}

				/* Process */
				Agent_Execute(Topic, Cmd, Data);

				/* Cleanup */
				if (Msg) free(Msg);

				break;
			}
			default:;
		}
	}


	/* Cleanup for quit */
	Agent_Cleanup();

	return 0;
}

void Agent_Stop()
{
	AllStop = 1;
}


