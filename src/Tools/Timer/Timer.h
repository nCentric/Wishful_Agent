/*
 * Timer.h
 *
 *  Created on: Oct 22, 2015
 *      Author: kvdp
 */

#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>
#include <time.h>

uint32_t Timer();

uint32_t TimerSpecToTimer(struct timespec* Ts);

void TimerSpecAddDelay(struct timespec* Ts, uint32_t Delay);

void TimerSpecGetMono(struct timespec* Ts);

void TimerSpecGetReal(struct timespec* Ts);

void TimerSpecGetMonoWithDelay(struct timespec* Ts, uint32_t Delay);

void TimerSpecGetRealWithDelay(struct timespec* Ts, uint32_t Delay);

#endif /* TIMER_H_ */
