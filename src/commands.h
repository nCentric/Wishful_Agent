/*
 * commands.h
 *
 *  Created on: Feb 10, 2017
 *      Author: kvdp
 */

#ifndef SRC_COMMANDS_H_
#define SRC_COMMANDS_H_

int cmd_InstallEgress(const char* Data);
int cmd_SetChannel(const char* Data, char** Reply);
int cmd_SetTxpower(const char* Data, char** Reply);
int cmd_SetFairness(const char* Data, char** Reply);
int cmd_Getchanmeas(char** Reply);
int cmd_Getlistrssi(char** Reply);
void cmd_Reboot();

#endif /* SRC_COMMANDS_H_ */
