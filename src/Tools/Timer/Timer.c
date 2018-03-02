/*
 * Timer.c
 *
 *  Created on: Oct 22, 2015
 *      Author: kvdp
 */

#include "Timer.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


uint32_t Timer()
{
	uint32_t CT;
	struct timespec Ts;

	if (clock_gettime(CLOCK_MONOTONIC, &Ts))
	{
		//Error
		printf("Time error: %s\n", strerror(errno));
		CT = 0;
	}
	else
	{
		CT = ((uint32_t) Ts.tv_sec * 1000) + ((uint32_t) Ts.tv_nsec / 1000000);
	}

	return CT;
}

uint32_t TimerSpecToTimer(struct timespec* Ts)
{
	return ((uint32_t) Ts->tv_sec * 1000) + ((uint32_t) Ts->tv_nsec / 1000000);
}

void TimerSpecAddDelay(struct timespec* Ts, uint32_t Delay)
{
	Ts->tv_sec += (Delay / 1000);
	Ts->tv_nsec += ((Delay % 1000) * 1000000);

	/* Handle overflow */
	if (Ts->tv_nsec >= 1000000000)
	{
		Ts->tv_sec++;
		Ts->tv_nsec -= 1000000000;
	}
}

void TimerSpecGetMono(struct timespec* Ts)
{
	clock_gettime(CLOCK_MONOTONIC, Ts);
}

void TimerSpecGetReal(struct timespec* Ts)
{
	clock_gettime(CLOCK_REALTIME, Ts);
}

void TimerSpecGetMonoWithDelay(struct timespec* Ts, uint32_t Delay)
{
	TimerSpecGetMono(Ts);
	TimerSpecAddDelay(Ts, Delay);
}

void TimerSpecGetRealWithDelay(struct timespec* Ts, uint32_t Delay)
{
	TimerSpecGetReal(Ts);
	TimerSpecAddDelay(Ts, Delay);
}

