/*********************************************************************************

 Copyright 2006 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

/*
	CONTROLLER.h

  Includes, etc. so make users don't have to

  MakingThings
*/

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "types.h"

#include "eeprom.h"
#include "analogin.h"
#include "rtos.h"
#include "pwm.h"
#include "io.h"
#include "led.h"
#include "timer.h"
#include "fasttimer.h"
#include "debug.h"
#include "can.h"
#include "osc.h"
#include "system.h"
#include "usb.h"
#include "network.h"
#include "stepper.h"


/* Make Helper Functions */

void Make( void );

void Timer0Isr( void );
void Timer0Init( void );
void Timer0Set( int t );
int Timer0Get( int* jitterTotal, int* jitterMax, int* jitterMaxAllDay );

void FastIsr( void );

void DisableFIQFromThumb( void );
void EnableFIQFromThumb( void );

#endif
