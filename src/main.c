#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent.h"

/* Def#include "Tools/Daemon/daemon.h"
initions */
#define MAX_PATH                255



#define PIDFILE "/var/run/wishagent.pid"
#define WISHFULAGENT_IPC_SOCKET "/tmp/wishagent.soc"



/* Internal functions */
void Intrpt(int Num)
{
	printf("Received an interrupt signal!\n");
	Agent_Stop();
}

void Error(int Num)
{
	printf("Received an error signal!\n");
	Agent_Stop();
}

//void Handler(int Num)
//{
//	printf("Received a handler signal!\n");
	//AllStop = 1;
//}

void Stop(int Num)
{
	printf("Received a stop signal!\n");
	Agent_Stop();
}

static void Usage()
{
    printf("wishagent - WiSHFUL Agent utility\n");
    printf("\n");
    printf("options:\n");
    printf("  -h               prints this help info\n");
}


/* MAIN */
int main(int argc, char *argv[])
{
    int c;
    const char* MyIP = NULL;
    const char* DLsocket = NULL;
    const char* ULsocket= NULL;

	//Signal handlers
	(void) signal(SIGINT, Intrpt);
	(void) signal(SIGHUP, Stop);
	(void) signal(SIGQUIT, Stop);
	(void) signal(SIGABRT, Stop);
	(void) signal(SIGKILL, Stop);
	(void) signal(SIGTERM, Stop);
	(void) signal(SIGSTOP, Stop);
	(void) signal(SIGILL, Error);
	(void) signal(SIGBUS, Error);
	(void) signal(SIGSEGV, Error);
	(void) signal(SIGCHLD, SIG_IGN);
	(void) signal(SIGPIPE, SIG_IGN);
	/* Parse Options */
    while ((c = getopt(argc, argv, "I:D:U:h")) != EOF)
    {
    	switch (c)
		{
			case 'I':	/* Config file to use */
			{
				MyIP = optarg;
				break;
			}

			case 'D':	/* Path to cache */
			{
				DLsocket = optarg;
				break;
			}

			case 'U':	/* Path to cache */
			{
				ULsocket = optarg;
				break;
			}

			case 'h':	/* Help */
			default:
			{
				Usage();
				exit(EXIT_SUCCESS);
			}
		}
    }

    Agent_Start(MyIP, DLsocket, ULsocket);

    return 0;
}
