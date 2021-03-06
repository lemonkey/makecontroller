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

// MakingThings - Make Controller Board - 2006

/** \file stepper.c	
	Stepper Motor Controls.
	Methods for controlling up to 2 stepper motors with the Make Application Board.
*/

#include "stepper.h"
#include "io.h"
#include "fasttimer.h"
#include "config.h"

#include "AT91SAM7X256.h"

#if ( APPBOARD_VERSION == 50 )
  #define STEPPER_0_IO_0 IO_PA02
  #define STEPPER_0_IO_1 IO_PA02
  #define STEPPER_0_IO_2 IO_PA02
  #define STEPPER_0_IO_3 IO_PA02
  #define STEPPER_1_IO_0 IO_PA02
  #define STEPPER_1_IO_1 IO_PA02
  #define STEPPER_1_IO_2 IO_PA02
  #define STEPPER_1_IO_3 IO_PA02
#endif
#if ( APPBOARD_VERSION == 90 || APPBOARD_VERSION == 95 || APPBOARD_VERSION == 100 )
  #define STEPPER_0_IO_0 IO_PA24
  #define STEPPER_0_IO_1 IO_PA05
  #define STEPPER_0_IO_2 IO_PA06
  #define STEPPER_0_IO_3 IO_PA02
  #define STEPPER_1_IO_0 IO_PB25
  #define STEPPER_1_IO_1 IO_PA25
  #define STEPPER_1_IO_2 IO_PA26
  #define STEPPER_1_IO_3 IO_PB23
#endif

#define STEPPER_COUNT 2

typedef struct StepperControlS
{
  int users;
  unsigned int bipolar : 1;
  unsigned int halfStep : 1;
  int speed;
  int duty;
  int acceleration;
  int positionRequested;
  int position;
  int io[ 4 ];  
  unsigned timerRunning : 1;
  FastTimerEntry fastTimerEntry;
} StepperControl;

typedef struct StepperS
{
  int users;
  StepperControl control[ STEPPER_COUNT ];
} Stepper_;

void Stepper_IRQCallback( int id );

static int Stepper_Start( int index );
static int Stepper_Stop( int index );
static int Stepper_Init( void );
static int Stepper_Deinit( void );
static int Stepper_GetIo( int index, int io );
static void Stepper_SetDetails( StepperControl* s );
static void Stepper_SetUnipolarHalfStepOutput( StepperControl *s, int position );
static void Stepper_SetUnipolarOutput( StepperControl *s, int position );
static void Stepper_SetBipolarOutput( StepperControl *s, int position );
static void Stepper_SetBipolarHalfStepOutput( StepperControl *s, int position );

void Stepper_SetOn( int index, int* portAOn, int* portBOn );
void Stepper_SetOff( int index, int* portAOff, int* portBOff );
void Stepper_SetAll( int portAOn, int portBOn, int portAOff, int portBOff );

Stepper_ Stepper;

/** \defgroup Stepper
* The Stepper Motor subsystem provides speed and position control for one or two stepper motors.
* Up to 2 stepper motors can be controlled with the MAKE Application Board.\n
* You may specify whether the stepper is bipolar or unipolar, and whether half steps are 
* taken or not.
* \ingroup AppBoard
* @{
*/

/**
	Attempt to set the specified 
	Sets whether the specified Stepper is active.
	@param index An integer specifying which stepper (0 or 1).
	@param state An integer specifying the active state - 1 (active) or 0 (inactive).
	@return Zero on success.
*/
int Stepper_SetActive( int index, int state )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( state )
    return Stepper_Start( index );
  else
    return Stepper_Stop( index );
}

/**
	Check whether the stepper is active or not
	@param index An integer specifying which stepper (0-1).
	@return State - 1 (active) or 0 (inactive).
*/
int Stepper_GetActive( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return false;
  return Stepper.control[ index ].users > 0;
}

/**	
	Set the position of the specified stepper motor.
	@param index An integer specifying which stepper (0 or 1).
	@param position An integer specifying the stepper position.
  @return status (0 = OK).
*/
int Stepper_SetPosition( int index, int position )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  StepperControl* s = &Stepper.control[ index ]; 
  
  DisableFIQFromThumb();
  s->position = position;
  EnableFIQFromThumb();

  Stepper_SetDetails( s );

  return CONTROLLER_OK;
}

/**	
	Set the position requested of the specified stepper motor.  If the motor is
  at rest at the previously requested position
	@param index An integer specifying which stepper (0 or 1).
	@param positionRequested An integer specifying the desired stepper position.
  @return status (0 = OK).
*/
int Stepper_SetPositionRequested( int index, int positionRequested )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  StepperControl* s = &Stepper.control[ index ]; 
  DisableFIQFromThumb();
  s->positionRequested = positionRequested;
  EnableFIQFromThumb();

  Stepper_SetDetails( s );

  return CONTROLLER_OK;
}

/**	
	Set the speed at which a stepper will move.
  This is a number of ms per step,
  rather than the more common steps per second.  Arranging it this way makes 
  it easier to express as an integer.  Fastest speed is 1ms / step (1000 steps
  per second) and slowest is many seconds.
	@param index An integer specifying which stepper (0 or 1).
	@param speed An integer specifying the stepper speed in ms per step
  @return status (0 = OK).
*/
int Stepper_SetSpeed( int index, int speed )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  StepperControl* s = &Stepper.control[ index ]; 
    
  if ( s->users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  s->speed = speed * 1000;

  DisableFIQFromThumb();
  FastTimer_SetTime( &s->fastTimerEntry, s->speed );
  EnableFIQFromThumb();

  Stepper_SetDetails( s );

  return CONTROLLER_OK;
}

/**	
	Get the speed at which a stepper will move.
	Read the value previously set for the speed parameter.
	@param index An integer specifying which stepper (0 or 1).
  @return The speed (0 - 1023), or 0 on error.
*/
int Stepper_GetSpeed( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].speed / 1000;
}

/**	
	Read the current position of a stepper motor.
	@param index An integer specifying which stepper (0 - 3).
  @return The position, 0 on error.
*/
int Stepper_GetPosition( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].position;
}

/**	
	Read the current requested position of a stepper motor.
	@param index An integer specifying which stepper (0 or 1).
  @return The position and 0 on error
*/
int Stepper_GetPositionRequested( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].positionRequested;
}

/**	
	Set the duty - from 0 to 1023.  The default is for 100% power (1023).
	@param index An integer specifying which stepper (0 or 1).
	@param duty An integer specifying the stepper duty (0 - 1023).
  @return status (0 = OK).
*/
int Stepper_SetDuty( int index, int duty )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  StepperControl* s = &Stepper.control[ index ]; 
  s->duty = duty;

  // Fire the PWM's up
  int pwm = index * 2;
  Pwm_Set( pwm, duty );
  Pwm_Set( pwm + 1, duty );

  return CONTROLLER_OK;
}

/**	
	Get the duty 
  Read the value previously set for the duty.
	@param index An integer specifying which stepper (0 - 3).
  @return The duty (0 - 1023), or 0 on error.
*/
int Stepper_GetDuty( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].duty;
}

/**	
	Declare whether the stepper is bipolar or not.  Default is unipolar.
	@param index An integer specifying which stepper (0 or 1).
	@param bipolar An integer 1 for bipolar, 0 for unipolar
  @return status (0 = OK).
*/
int Stepper_SetBipolar( int index, int bipolar )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  StepperControl* s = &Stepper.control[ index ]; 
  s->bipolar = bipolar;

  return CONTROLLER_OK;
}

/**	
	Get the bipolar setting
  Read the value previously set for bipolar.
	@param index An integer specifying which stepper (0 - 3).
  @return 1 for bipolar or 0 for unipolar.
*/
int Stepper_GetBipolar( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].bipolar;
}

/**	
	Declare whether the stepper is in half stepping mode or not.  Default is not - i.e. in full step mode.
	@param index An integer specifying which stepper (0 or 1).
	@param halfStep An integer specifying 1 for half step, 0 for full step
  @return status (0 = OK).
*/
int Stepper_SetHalfStep( int index, int halfStep )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return status;
  }

  StepperControl* s = &Stepper.control[ index ]; 
  s->halfStep = halfStep;

  return CONTROLLER_OK;
}

/**	
	Get the HalfStep value 
  Read the value previously set for the HalfStep.
	@param index An integer specifying which stepper (0 or 1).
  @return the HalfStep setting.
*/
int Stepper_GetHalfStep( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return 0;

  if ( Stepper.control[ index ].users < 1 )
  {
    int status = Stepper_Start( index );
    if ( status != CONTROLLER_OK )
      return 0;
  }

  return Stepper.control[ index ].halfStep;
}


/** @}
*/

int Stepper_Start( int index )
{
  int status;

  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  StepperControl* sc = &Stepper.control[ index ]; 
  if ( sc->users++ == 0 )
  {
    if ( Stepper.users++ == 0 )
    {
      int status = Stepper_Init();
      if ( status != CONTROLLER_OK )
      {
        // I guess not then
        sc->users--;
        Stepper.users--;
        return status;
      }   
    }

    status = Pwm_Start( index * 2 );
    if ( status != CONTROLLER_OK )
    {
      sc->users--;
      Stepper_Deinit();
      Stepper.users--;
      return status;
    }

    status = Pwm_Start( index * 2 + 1 );
    if ( status != CONTROLLER_OK )
    {
      Pwm_Stop( index * 2 );
      sc->users--;
      Stepper_Deinit();
      Stepper.users--;
      return status;
    }

    // Fire the PWM's up
    sc->duty = 1023;

    int pwm = index * 2;
    Pwm_Set( pwm, sc->duty );
    Pwm_Set( pwm + 1, sc->duty );

    // Get IO's
    int i;
    for ( i = 0; i < 4; i++ )
    {
      int io = Stepper_GetIo( index, i );      
      sc->io[ i ] = io;
        
      // Try to lock the stepper output
      status = Io_Start( io, true );
      if ( status != CONTROLLER_OK )
      {
        // Damn - unlock any that we did get
        int j;
        for ( j = 0; j < i; j++ )
          Io_Stop( sc->io[ j ] );

        Pwm_Stop( pwm );
        Pwm_Stop( pwm + 1 );

        Stepper.users--;
        sc->users--;
        return status;
      }
      Io_PioEnable( io );
      Io_SetTrue( io );
      Io_SetOutput( io );
    }

    DisableFIQFromThumb();
    sc->position = 0;
    sc->positionRequested = 0;
    sc->speed = 1000;
    sc->halfStep = false;
    sc->bipolar = false;
    EnableFIQFromThumb();

    FastTimer_InitializeEntry( &sc->fastTimerEntry, Stepper_IRQCallback, index, sc->speed * 1000, true );
    // FastTimer_Set( &sc->fastTimerEntry );
  }

  return CONTROLLER_OK;
}

int Stepper_Stop( int index )
{
  if ( index < 0 || index >= STEPPER_COUNT )
    return CONTROLLER_ERROR_ILLEGAL_INDEX;

  StepperControl* s = &Stepper.control[ index ]; 

  if ( s->users <= 0 )
    return CONTROLLER_ERROR_TOO_MANY_STOPS;

  if ( --s->users == 0 )
  {
    if ( s->timerRunning )
    {
      DisableFIQFromThumb();
      FastTimer_Cancel( &s->fastTimerEntry );
      EnableFIQFromThumb();
    }

    int i;
    for ( i = 0; i < 4; i++ )
    {
      int io = s->io[ i ];
      Io_SetFalse( io );
      Io_Stop( io );
    }
 
    int pwm = index * 2;
    Pwm_Stop( pwm );
    Pwm_Stop( pwm + 1 );

    if ( --Stepper.users == 0 )
    {
      Stepper_Deinit();
    }
  }

  return CONTROLLER_OK;
}


int Stepper_GetIo( int stepperIndex, int ioIndex )
{
  int io = -1;
  switch ( stepperIndex )
  {
    case 0:
      switch ( ioIndex )
      {
        case 0:
          io = STEPPER_0_IO_0;
          break;
        case 1:
          io = STEPPER_0_IO_1;
          break;
        case 2:
          io = STEPPER_0_IO_2;
          break;
        case 3:
          io = STEPPER_0_IO_3;
          break;
      }
      break;
    case 1:
      switch ( ioIndex )
      {
        case 0:
          io = STEPPER_1_IO_0;
          break;
        case 1:
          io = STEPPER_1_IO_1;
          break;
        case 2:
          io = STEPPER_1_IO_2;
          break;
        case 3:
          io = STEPPER_1_IO_3;
          break;
      }
      break;
  }
  return io;
}

int Stepper_Init()
{
  // FastTimer_InitializeEntry( &Stepper.fastTimerEntry, Stepper_IRQCallback, 0, 100000, true );
  // FastTimer_Set( &Stepper.fastTimerEntry );

  return CONTROLLER_OK;
}

int Stepper_Deinit()
{
  // Disable the device
  // AT91C_BASE_TC1->TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;
  return CONTROLLER_OK;
}

void Stepper_IRQCallback( int id )
{
  StepperControl* s = &Stepper.control[ id ]; 

  if ( s->position < s->positionRequested )
    s->position++;
  if ( s->position > s->positionRequested )
    s->position--;

  if ( s->bipolar )
  {
    if ( s->halfStep )
      Stepper_SetBipolarHalfStepOutput( s, s->position );
    else
      Stepper_SetBipolarOutput( s, s->position );
  }
  else
  {
    if ( s->halfStep ) 
      Stepper_SetUnipolarHalfStepOutput( s, s->position );
    else
      Stepper_SetUnipolarOutput( s, s->position );
  }

  if ( s->position == s->positionRequested )
  {
    FastTimer_Cancel( &s->fastTimerEntry );
    s->timerRunning = false;
  }
}

void Stepper_SetDetails( StepperControl* s )
{
  if ( !s->timerRunning && ( s->position != s->positionRequested ) && ( s->speed != 0 ) )
  {
    s->timerRunning = true;
    FastTimer_Set( &s->fastTimerEntry );
  }
  else
  {
    if ( ( s->timerRunning ) && ( ( s->position == s->positionRequested ) || ( s->speed == 0 ) ) )
    {
      DisableFIQFromThumb();
      FastTimer_Cancel( &s->fastTimerEntry );
      EnableFIQFromThumb();
      s->timerRunning = false;
    }
  }
}

void Stepper_SetUnipolarHalfStepOutput( StepperControl *s, int position )
{
  int output = position % 8;

  int* iop = s->io;

  int portAOn = 0;
  int portBOn = 0;
  int portAOff = 0;
  int portBOff = 0;

  switch ( output )
  {
    case -1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 0:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 1:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 2:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 3:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 4:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 5:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
    case 6:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
    case 7:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
  }  

  Stepper_SetAll( portAOn, portBOn, portAOff, portBOff );
}

void Stepper_SetUnipolarOutput( StepperControl *s, int position )
{
  int output = position % 4;
  int* iop = s->io;

  int portAOn = 0;
  int portBOn = 0;
  int portAOff = 0;
  int portBOff = 0;

  switch ( output )
  {
    case -1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 0:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 2:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 3:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
  }  
  Stepper_SetAll( portAOn, portBOn, portAOff, portBOff );
}

void Stepper_SetBipolarHalfStepOutput( StepperControl *s, int position )
{
  int output = position % 8;

  int* iop = s->io;

  int portAOn = 0;
  int portBOn = 0;
  int portAOff = 0;
  int portBOff = 0;

  switch ( output )
  {
    case -1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 0:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 1:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 2:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 3:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 4:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 5:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
    case 6:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
    case 7:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOff( *iop++, &portAOff, &portBOff );      
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
  }  

  Stepper_SetAll( portAOn, portBOn, portAOff, portBOff );
}

void Stepper_SetBipolarOutput( StepperControl *s, int position )
{
  int output = position % 4;
  int* iop = s->io;

  int portAOn = 0;
  int portBOn = 0;
  int portAOff = 0;
  int portBOff = 0;

  // This may be the least efficient code I have ever written
  switch ( output )
  {
    case -1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 0:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 1:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      break;
    case 2:
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
    case 3:
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOff( *iop++, &portAOff, &portBOff );
      Stepper_SetOn( *iop++, &portAOn, &portBOn );
      break;
  }  
  Stepper_SetAll( portAOn, portBOn, portAOff, portBOff );
}

void Stepper_SetOn( int index, int* portAOn, int* portBOn )
{
  int mask = 1 << ( index & 0x1F );
  if ( index < 32 )
    *portAOn |= mask;
  else
    *portBOn |= mask;
}

void Stepper_SetOff( int index, int* portAOff, int* portBOff )
{
  int mask = 1 << ( index & 0x1F );
  if ( index < 32 )
    *portAOff |= mask;
  else
    *portBOff |= mask;
}

void Stepper_SetAll( int portAOn, int portBOn, int portAOff, int portBOff )
{
  AT91C_BASE_PIOA->PIO_SODR = portAOn;
  AT91C_BASE_PIOB->PIO_SODR = portBOn;
  AT91C_BASE_PIOA->PIO_CODR = portAOff;
  AT91C_BASE_PIOB->PIO_CODR = portBOff;
}

#ifdef OSC // defined in config.h

/** \defgroup StepperOSC Stepper - OSC
  Control Stepper motors with the Application Board via OSC.  Internally it uses the PWM subsystem
  to control duty and the FastTimer to generate the very accurate timings for the steps.
  \ingroup OSC
	
	\section devices Devices
	There are 2 Stepper controllers available on the Application Board, numbered 0 & 1.\n
	See the Stepper section in the Application Board user's guide for more information
	on hooking steppers up to the board.
	
	\section properties Properties
	Each stepper controller has seven properties - 'position', 'positionrequested', 'speed', 'duty', 'bipolar', 
  'halfstep' and 'active'.

	\par Position
	The 'position' property corresponds to the current step position of the stepper motor
	This value can be both read and written.  Writing this value changes where the motor thinks it is.
  The initial value of this parameter is 0.
	\par
	To set the first stepper to step position 10000, send the message
	\verbatim /stepper/0/position 10000\endverbatim
	Leave the argument value off to read the position of the stepper:
	\verbatim /stepper/0/position \endverbatim

  \par PositionRequested
	The 'positionrequested' property describes the desired step position of the stepper motor
	This value can be both read and written.  Writing this value changes where the motor thinks it needs to be.
  The initial value of this parameter is 0.
	\par
	To set the first stepper to go to step position 10000, send the message
	\verbatim /stepper/0/positionrequested 10000\endverbatim
	Leave the argument value off to read the last requested position of the stepper:
	\verbatim /stepper/0/positionrequested \endverbatim

	\par Speed
	The 'speed' property corresponds to the speed with which the stepper responds to changes 
	of position.
	This value can be both read and written, and the range of values is 0 - 71000.  A speed of 1
	means the stepper performs a new step every millisecond.  Bigger numbers result in slower 
  steps.
	\par
	To set the speed of the first stepper to step at 100ms per step, send a message like
	\verbatim /stepper/0/speed 100 \endverbatim
	Adjust the argument value to one that suits your application.\n
	Leave the argument value off to read the speed of the stepper:
	\verbatim /stepper/0/speed \endverbatim
	
  \par Duty
	The 'duty' property corresponds to the how much of the power supply is to be sent to the
  stepper.  This is handy for when the stepper is static and not being required to perform too
  much work and reducing its power helps reduce heat dissipation.
	This value can be both read and written, and the range of values is 0 - 1023.  A duty of 0
	means the stepper gets no power, and the value of 1023 means the stepper gets full power.  
  \par
	To set the duty of the first stepper to 500, send a message like
	\verbatim /stepper/0/speed 500 \endverbatim
	Adjust the argument value to one that suits your application.\n
	Leave the argument value off to read the duty of the stepper:
	\verbatim /stepper/0/speed \endverbatim

  \par Bipolar
	The 'bipolar' property is set to the style of stepper being used.  A 0 here implies unipolar
  (the default) and 1 implies a bipolar stepper.
	This value can be both read and written.

  \par HalfStep
	The 'halfstep' property controls whether the stepper is being half stepped or not.  A 0 here implies full stepping
  (the default) and 1 implies a half stepping.
	This value can be both read and written.

	\par Active
	The 'active' property corresponds to the active state of the stepper.
	If the stepper is set to be active, no other tasks will be able to
	write to the same I/O lines.  If you're not seeing appropriate
	responses to your messages to a stepper, check the whether it's 
	locked by sending a message like
	\verbatim /stepper/1/active \endverbatim
*/

#include "osc.h"
#include "string.h"
#include "stdio.h"

// Need a list of property names
// MUST end in zero
static char* StepperOsc_Name = "stepper";
static char* StepperOsc_PropertyNames[] = { "active", "position", "positionrequested", "speed", "duty", "halfstep", "bipolar", 0 }; // must have a trailing 0

int StepperOsc_PropertySet( int index, int property, int value );
int StepperOsc_PropertyGet( int index, int property );

// Returns the name of the subsystem
const char* StepperOsc_GetName( )
{
  return StepperOsc_Name;
}

// Now getting a message.  This is actually a part message, with the first
// part (the subsystem) already parsed off.
int StepperOsc_ReceiveMessage( int channel, char* message, int length )
{
  int status = Osc_IndexIntReceiverHelper( channel, message, length, 
                                           STEPPER_COUNT, StepperOsc_Name,
                                           StepperOsc_PropertySet, StepperOsc_PropertyGet, 
                                           StepperOsc_PropertyNames );

  if ( status != CONTROLLER_OK )
    return Osc_SendError( channel, StepperOsc_Name, status );
  return CONTROLLER_OK;
}

// Set the index LED, property with the value
int StepperOsc_PropertySet( int index, int property, int value )
{
  switch ( property )
  {
    case 0:
      Stepper_SetActive( index, value );
      break;
    case 1:
      Stepper_SetPosition( index, value );
      break;
    case 2:
      Stepper_SetPositionRequested( index, value );
      break;
    case 3:
      Stepper_SetSpeed( index, value );
      break;
    case 4:
      Stepper_SetDuty( index, value );
      break;
    case 5:
      Stepper_SetHalfStep( index, value );
      break;
    case 6:
      Stepper_SetBipolar( index, value );
      break;
  }
  return CONTROLLER_OK;
}

// Get the index LED, property
int StepperOsc_PropertyGet( int index, int property )
{
  int value;
  switch ( property )
  {
    case 0:
      value = Stepper_GetActive( index );
      break;
    case 1:
      value = Stepper_GetPosition( index );
      break;
    case 2:
      value = Stepper_GetPositionRequested( index );
      break;
    case 3:
      value = Stepper_GetSpeed( index );
      break;
    case 4:
      value = Stepper_GetDuty( index );
      break;
    case 5:
      value = Stepper_GetHalfStep( index );
      break;
    case 6:
      value = Stepper_GetBipolar( index );
      break;
  }
  
  return value;
}

#endif
