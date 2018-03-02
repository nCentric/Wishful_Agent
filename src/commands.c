/*
 * commands.c
 *
 *  Created on: Feb 10, 2017
 *      Author: kvdp
 */

#include "commands.h"

#include <arpa/inet.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ExecuteTxtCmd(const char* cmd, char* Reply, int RepSz)
{
	FILE* f;
	char* Ln = NULL;
	size_t Len = 0;
	int Pos = 0;

	/* Start Process */
	if (!(f = popen(cmd, "r")))
	{
		return -1 ;
	}

	/* Read command output */
	while (getline(&Ln, &Len, f) != -1)
	{
		if ((Pos += snprintf(&Reply[Pos], RepSz - Pos, "%s", Ln)) >= RepSz)
		{
			break;	/* Buff is full */
		}
	}

	/* Cleanup */
	if (Ln) free(Ln);
	pclose(f);

	return 0;
}



/* Translate incoming command to action */
int cmd_InstallEgress(const char* Data)
{

	return 0;
}

int cmd_SetChannel(const char* Data, char** Reply)
{
	int stat = 0;
	json_object* jobj = json_tokener_parse(Data);
	json_object* jobj_chan;

	/* Check incoming data */
	if (json_object_object_get_ex(jobj, "channel", &jobj_chan)	/* Key exists ? */
	&&  json_object_is_type(jobj_chan, json_type_int))			/* Data should be INT */
	{
		int channel;
		char cmd[512] = {0};

		/* Get channel nr */
		channel = json_object_get_int(jobj_chan);

		/* Cleanup */
		while (!json_object_put(jobj_chan));
		while (!json_object_put(jobj));

		/* Compose QCM cmd */
		snprintf(cmd, sizeof(cmd), "QueryCoreMain2 !CHAN%d", channel);

		/* Execute */
		if (!ExecuteTxtCmd(cmd, cmd, sizeof (cmd) -1)
		&& strstr(cmd, "OK!"))
		{
			stat = 1;
		}
	}

	/* Convert to JSON objects */
	struct json_object* robj = json_object_new_object();
	struct json_object* j_acknack = json_object_new_int(stat);

	/* Assemble */
	json_object_object_add(robj, "channel", j_acknack);

	/* Get string */
	*Reply = strdup(json_object_get_string(robj));

	/* cleanup */
	while (!json_object_put(robj));

	if (!*Reply)
	{
		return -1;
	}

	return 0;
}

int cmd_SetTxpower(const char* Data, char** Reply)
{
	int stat = 0;
	json_object* jobj = json_tokener_parse(Data);
	json_object* jobj_pow;


	/* Check incoming data */
	if (json_object_object_get_ex(jobj, "txpower", &jobj_pow)	/* Key exists ? */
	&&  json_object_is_type(jobj_pow, json_type_int))			/* Data should be INT */
	{
		int txpower;
		char cmd[512] = {0};

		/* Get txpower */
		txpower = json_object_get_int(jobj_pow);

		/* Cleanup */
		while (!json_object_put(jobj_pow));
		while (!json_object_put(jobj));

		/* Compose QCM cmd */
		snprintf(cmd, sizeof(cmd), "QueryCoreMain2 !TXPOW%d", txpower);

		/* Execute */
		if (!ExecuteTxtCmd(cmd, cmd, sizeof (cmd) -1)
		&& strstr(cmd, "OK!"))
		{
			stat = 1;
		}
	}

	/* Convert to JSON objects */
	struct json_object* robj = json_object_new_object();
	struct json_object* j_acknack = json_object_new_int(stat);

	/* Assemble */
	json_object_object_add(robj, "txpower", j_acknack);

	/* Get string */
	*Reply = strdup(json_object_get_string(robj));

	/* cleanup */
	while (!json_object_put(robj));

	if (!*Reply)
	{
		return -1;
	}

	return 0;
}

int cmd_SetFairness(const char* Data, char** Reply)
{
	int stat = 0;
	json_object* jobj = json_tokener_parse(Data);
	json_object* jobj_fair;
	char cmd[128];

	/* Check incoming data */
	if (json_object_object_get_ex(jobj, "airtimefairness", &jobj_fair)	/* Key exists ? */
	&&  json_object_is_type(jobj_fair, json_type_int))					/* Data should be INT */
	{
		int fairness;

		/* Get flag */
		fairness = json_object_get_int(jobj_fair);

		/* Cleanup */
		while (!json_object_put(jobj_fair));
		while (!json_object_put(jobj));

		/* Compose cmd */
		snprintf(cmd, sizeof(cmd), "for f in /sys/kernel/debug/ieee80211/*; do echo %d > $f/ath9k/airtime_flags; done", (fairness > 0) ? 7 : 0);

		/* Execute */
		system(cmd);

		stat = 1;
	}

	/* Convert to JSON objects */
	struct json_object* robj = json_object_new_object();
	struct json_object* j_acknack = json_object_new_int(stat);

	/* Assemble */
	json_object_object_add(robj, "airtimefairness", j_acknack);

	/* Get string */
	*Reply = strdup(json_object_get_string(robj));

	/* cleanup */
	while (!json_object_put(robj));

	if (!*Reply)
	{
		return -1;
	}

	return 0;
}

int cmd_Getchanmeas(char** Reply)
{
	char cmd[512];

	/* Compose cmd */
	snprintf(cmd, sizeof(cmd), "QueryCoreMain2 ?MEAS");

	/* Execute */
	if (ExecuteTxtCmd(cmd, cmd, sizeof (cmd) -1))
	{
		return -1;
	}

	/* Fetch from reply */
	int channel = 0;
	int frequency = 0;
	int noisefloor = 0;
	double load = 0.0;
	double load_wifi = 0.0;
	double load_noise = 0.0;

	if (sscanf(cmd, "Channel: %d\nFreq: %d MHz\nNoiseFloor: %d dBm\nLoad: %lf%%\n\tWiFi: %lf%%\nNoise: %lf", &channel, &frequency, &noisefloor, &load, &load_wifi, &load_noise) != 6)
	{
		*Reply = strdup("Error during chanmeas!");
		return -1;
	}

	/* Convert to JSON objects */
	struct json_object* robj = json_object_new_object();
	struct json_object* j_chan = json_object_new_int(channel);
	struct json_object* j_load = json_object_new_double(load);
	struct json_object* J_lono = json_object_new_double(load_noise);

	/* Assemble */
	json_object_object_add(robj, "chan", j_chan);
	json_object_object_add(robj, "occup", j_load);
	json_object_object_add(robj, "noise", J_lono);

	/* Get string */
	*Reply = strdup(json_object_get_string(robj));

	/* cleanup */
	while (!json_object_put(robj));

	if (!*Reply)
	{
		return -1;
	}

	return 0;
}

int cmd_Getlistrssi(char** Reply)
{
	int Idx = 0;
	int Start = 0;
	int Nr = 0;
	char cmd[1024];

	/* Compose cmd */
	snprintf(cmd, sizeof(cmd), "QueryCoreMain2 ?LIN | grep \"10.254.253.\"");

	/* Execute */
	if (ExecuteTxtCmd(cmd, cmd, sizeof (cmd) -1))
	{
		return -1;
	}

	struct json_object* robj = json_object_new_object();
	struct json_object* robj_list = json_object_new_array();

	while (cmd[Idx])
	{
		if (cmd[Idx] == '\n')
		{
			int conv;
			char Dest[32] = {0};
			int l_rssi = 0;
			int r_rssi = 0;

			/* Insert NULL */
			cmd[Idx] = 0;

			/* Process line */
			conv = sscanf(&cmd[Start], "%s %*s %*s %d/%d", Dest, &l_rssi, &r_rssi);

			if (conv >= 3)
			{
				uint32_t IP = 0;

				/* Keep worst RSSI */
				if (r_rssi < l_rssi) l_rssi = r_rssi;
				inet_pton(AF_INET, Dest, &IP);

				/* Convert to ID */
				IP = ntohl(IP) & 0xFF;

				/* Add to JSON */
				struct json_object* mainobj = json_object_new_object();
				struct json_object* j_id = json_object_new_int(IP);
				struct json_object* j_rssi = json_object_new_int(l_rssi);

				json_object_object_add(mainobj, "node", j_id);
				json_object_object_add(mainobj, "rssi", j_rssi);
				json_object_array_add(robj_list, mainobj);

				/* Inc. Amount found */
				++Nr;
			}

			/* Set new start after fake NULL */
			Start = Idx + 1;
		}

		++Idx;
	}

	/* Assemble */
	json_object_object_add(robj, "rssilist", robj_list);

	/* Get string */
	*Reply = strdup(json_object_get_string(robj));

	/* cleanup */
	while (!json_object_put(robj));

	if (!*Reply)
	{
		return -1;
	}

	return 0;
}

void cmd_Reboot()
{
	system("reboot");
}

