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

#ifndef MC_OSC_H
#define MC_OSC_H

#define OSC_MAX_MESSAGE 2048
#define OSC_MAX_ARG_COUNT 10

#include "mcError.h"
#include "ext.h"

#include <stdarg.h>
#include <stdlib.h>

typedef struct
{
  char packetBuf[1500];
  int length;
} t_osc_packet;

typedef struct // aligned with the way Max sends out messages
{
  t_symbol* address; // there's the address, that needs to be put in here with gensym( )
  short argc; // number of args
  t_atom argv[ OSC_MAX_ARG_COUNT ]; // the args themselves, as t_atoms
} t_osc_message;

typedef struct
{
  char outBuffer[ OSC_MAX_MESSAGE ];
  char* outBufferPointer;
  int outBufferRemaining;
  int outMessageCount;
} t_osc;

#include "usb_serial.h"

char* writePaddedString( char* buffer, int* length, char* string );
unsigned int endianSwap( unsigned int a );
char* Osc_writeTimetag( t_osc* o, char* buffer, int* length, int a, int b );
char* Osc_createBundle( t_osc* o, char* buffer, int* length, int a, int b );
mcError Osc_create_message( t_osc* o, char* address, int ac, t_atom* av );
int Osc_extractData( t_osc* o, char* buffer, t_osc_message* osc_message );
void Osc_receive_packet( void* out, t_osc* o, char* packet, int length, t_osc_message* osc_message );
void Osc_receive_message( t_osc* o, char* in, int length, t_osc_message* osc_message );
char* Osc_find_data_tag( t_osc* o, char* message, int length );
char* Osc_create_message_internal( t_osc* o, char* bp, int* length, char* address, int ac, t_atom* av );
mcError Osc_send_packet( t_osc* o, t_usbInterface* x, char* packet, int length );
void Osc_resetOutBuffer( t_osc* o );
void Osc_reset_message( t_osc_message* msg );

#endif	


