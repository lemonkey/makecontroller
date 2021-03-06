/*********************************************************************************

 Copyright 2006-2009 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

#include "analogin.h"
#include "core.h"

#define ANALOGIN_0 PIN_PB27
#define ANALOGIN_1 PIN_PB28
#define ANALOGIN_2 PIN_PB29
#define ANALOGIN_3 PIN_PB30

#define ANALOGIN_CHANNELS 8

struct AinDriver {
  Mutex mtx;                   // lock for the adc system
  Thread *thd;
  bool processMultiChannelIsr; // are we waiting for a multi conversion or just a single channel
  uint8_t multiChannelConversions; // mask of which conversions have been completed
};

static struct AinDriver aind;

#ifdef OSC
static void analoginAutoSendInit(void);
#endif

/**
  \defgroup analogin Analog Input
  10-bit analog inputs.
  The analog inputs read incoming signals from 0 - 3.3V.  They are rated as 5V tolerant, 
  but will not return meaningful values for anything above 3.3V.
  
  \section Usage
  ainInit() is called during system startup, so you can start reading via ainValue() 
  whenever you like.  If you don't want it to be initialized automatically, define
  \b NO_AIN_INIT in your config.h file.
  
  \section Values
  Analog inputs will return a value between 0 and 1023, corresponding to the range of \b 0 to \b 3.3V on the input.
  
  If you want to convert this to the actual voltage, you can use the following conversion:
  \code float voltage = 3.3 * (ainValue(1) / 1023.0); \endcode
  where \b ainValue is the AnalogIn value.
  
  A quicker version that doesn't use floating point, but will be slightly less precise:
  \code int voltage = (100 * ainValue(1)) / 1023; \endcode
  \ingroup io
  @{
*/

/** 
  Read the value of an analog input.
  @param channel Which analog in to sample - valid options are 0-7.
  @return The value as an integer (0 - 1023).
  
  \b Example
  \code
  if (ainValue(0) > 500) {
     // then do this
  }
  \endcode
*/
int analoginValue(int channel)
{
  int value;
  chSysLock();
  chMtxLockS(&aind.mtx);
  aind.processMultiChannelIsr = NO;
  // disable other channels, and enable the one we want
  AT91C_BASE_ADC->ADC_CHDR = ~(1 << channel);
  AT91C_BASE_ADC->ADC_CHER = (1 << channel);
  AT91C_BASE_ADC->ADC_CR = AT91C_ADC_START; // Start the conversion

  aind.thd = chThdSelf();
  chSchGoSleepS(THD_STATE_SUSPENDED);
  // this thread gets rescheduled from the ISR
  value = chThdSelf()->p_u.rdymsg;
  chMtxUnlockS();
  chSysUnlock();
  return value;
}

/** 
  Read the value of all the analog inputs.
  If you want to read all the analog ins, this is quicker than reading them all
  separately.  Make sure to provide an array of 8 ints, as this does not do
  any checking about the size of the array it's writing to.
  @param values An array of ints to be filled with the values.
  @return non-zero on success, zero on failure.
  
  \b Example
  \code
  int samples[8];
  ainMulti(samples);
   // now samples is filled with all the analogin values
  \endcode
*/
bool analoginMulti(int values[])
{
  chSysLock();
  chMtxLockS(&aind.mtx);
  // enable all the channels
  AT91C_BASE_ADC->ADC_CHER = 0xFF; // channel enables are the low byte

  aind.processMultiChannelIsr = YES;        // how to process the ISR
  aind.multiChannelConversions = 0;         // which channels have completed
  AT91C_BASE_ADC->ADC_CR = AT91C_ADC_START; // start the conversion

  aind.thd = chThdSelf();
  chSchGoSleepS(THD_STATE_SUSPENDED);
  // this thread gets rescheduled from the ISR

  values[0] = AT91C_BASE_ADC->ADC_CDR0;
  values[1] = AT91C_BASE_ADC->ADC_CDR1;
  values[2] = AT91C_BASE_ADC->ADC_CDR2;
  values[3] = AT91C_BASE_ADC->ADC_CDR3;
  values[4] = AT91C_BASE_ADC->ADC_CDR4;
  values[5] = AT91C_BASE_ADC->ADC_CDR5;
  values[6] = AT91C_BASE_ADC->ADC_CDR6;
  values[7] = AT91C_BASE_ADC->ADC_CDR7;
  
  chMtxUnlockS();
  chSysUnlock();
  return true;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void analoginServeInterrupt(void)
{
  uint32_t status = AT91C_BASE_ADC->ADC_SR;
  if (aind.processMultiChannelIsr) {
    aind.multiChannelConversions |= (status & 0xFF); // EoC channels are the low byte
    // if we got End Of Conversion in all our channels, indicate we're done
    if (aind.multiChannelConversions == 0xFF && aind.thd) {
      chSysLockFromIsr();
      // dummy read to clear the AT91C_ADC_DRDY bit
      (void)AT91C_BASE_ADC->ADC_LCDR;
      chSchReadyI(aind.thd);
      aind.thd = 0;
      chSysUnlockFromIsr();
    }
  }
  else if ((status & AT91C_ADC_DRDY) && (aind.thd != 0)) {
    chSysLockFromIsr();
    // send a msg back to the calling thread with the conversion result
    aind.thd->p_u.rdymsg = AT91C_BASE_ADC->ADC_LCDR & 0xFFFF;
    chSchReadyI(aind.thd);
    aind.thd = 0;
    chSysUnlockFromIsr();
  }
}

static CH_IRQ_HANDLER(analoginIsr)
{
  CH_IRQ_PROLOGUE();
  analoginServeInterrupt();
  AT91C_BASE_AIC->AIC_EOICR = 0;
  CH_IRQ_EPILOGUE();
}

/**
  Initialize the analog in system.
*/
void analoginInit()
{
  AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_ADC; // enable the peripheral clock
  AT91C_BASE_ADC->ADC_CR = AT91C_ADC_SWRST;     // reset to clear out previous settings
  
  // ADCClock = MCK / ( (PRESCAL+1) * 2 )
  // Startup Time = (STARTUP+1) * 8 / ADCClock
  // Sample & Hold Time = SHTIM/ADCClock

  // prescal = (mckClock / (2*adcClock)) - 1;
  // startup = ((adcClock/1000000) * startupTime / 8) - 1;
  // shtim = (((adcClock/1000000) * sampleAndHoldTime)/1000) - 1;
  
  // Set up
  AT91C_BASE_ADC->ADC_MR =
       AT91C_ADC_TRGEN_DIS |                // Hardware Trigger Disabled
       AT91C_ADC_LOWRES_10_BIT |            // 10 bit conversion
       AT91C_ADC_SLEEP_NORMAL_MODE |        // normal mode (no SLEEP)
       ((9 << 8)    & AT91C_ADC_PRESCAL) |  // Prescale rate (8 bits)
       ((127 << 16) & AT91C_ADC_STARTUP) |  // Startup rate
       ((127 << 24) & AT91C_ADC_SHTIM);     // Sample and Hold Time
   
  // initialize non-adc pins
  // pins ADC4-7 can only ever be ADCs (not full GPIOs) so no need to configure them
  pinGroupSetMode(GROUP_B, ANALOGIN_0 | ANALOGIN_1 | ANALOGIN_2 | ANALOGIN_3, PAL_MODE_INPUT_ANALOG);
  
  // init locks
  aind.thd = 0;
  chMtxInit(&aind.mtx);
  aind.multiChannelConversions = NO;
  aind.processMultiChannelIsr = NO;
  
  // initialize interrupts
  AT91C_BASE_ADC->ADC_IER = AT91C_ADC_DRDY;
  AIC_ConfigureIT(AT91C_ID_ADC, AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | 4, analoginIsr);
  AIC_EnableIT(AT91C_ID_ADC);

  #ifdef OSC
  analoginAutoSendInit();
  #endif
}

/**
  Deinitialize the analog in system.
*/
void analoginDeinit()
{
  AT91C_BASE_PMC->PMC_PCDR = 1 << AT91C_ID_ADC; // disable peripheral clock
  AIC_DisableIT(AT91C_ID_ADC);                  // disable interrupts
}

/** @}
*/

#ifdef OSC

#include <stdio.h>

/** \defgroup AnalogInOSC Analog In - OSC
  Read the Application Board's Analog Inputs via OSC.
  \ingroup OSC
  
  \section devices Devices
  There are 8 Analog Inputs on the Make Application Board, numbered 0 - 7.
  
  \section properties Properties
  The Analog Ins have the following properties:
  - value
  - autosend

  \par Value
  The \b value property corresponds to the incoming signal of an Analog In.
  The range of values you can expect to get back are from 0 - 1023.
  Because you can only ever \em read the value of an input, you'll never
  want to include an argument at the end of your OSC message to read the value.\n
  To read the sixth Analog In, send the message
  \verbatim /analogin/5/value \endverbatim
  The board will then respond by sending back an OSC message with the Analog In value.
  
  \par Autosend
  The \b autosend property corresponds to whether an analogin will automatically send a message
  when its incoming value changes.
  To tell the Controller to automatically send messages from analogin 4, send the message
  \verbatim /analogin/5/autosend 1 \endverbatim
  To have the Controller stop sending messages from analogin 4, send the message
  \verbatim /analogin/5/autosend 0 \endverbatim
  All autosend messages send at the same interval.  You can set this interval, in 
  milliseconds, by sending the message
  \verbatim /system/autosend-interval 10 \endverbatim
  so that messages will be sent every 10 milliseconds.  This can be anywhere from 1 to 5000 milliseconds.
  \par
  You also need to select whether the board should send to you over USB or Ethernet.  Send
  \verbatim /system/autosend-usb 1 \endverbatim
  to send via USB, and 
  \verbatim /system/autosend-udp 1 \endverbatim
  to send via Ethernet.  Via Ethernet, the board will send messages to the last address it received a message from.
*/

// sort of a checksum to verify whether a previous save was legit
#define AIN_AUTOSEND_SAVED 0xDF

static int analoginAutosendVals[ANALOGIN_CHANNELS];
static uint16_t analoginAutosendChannels;

void analoginAutoSendInit()
{
  analoginAutosendChannels = eepromRead(EEPROM_ANALOGIN_AUTOSEND);
  if (((analoginAutosendChannels >> 8) & 0xFF) != AIN_AUTOSEND_SAVED)
    analoginAutosendChannels = AIN_AUTOSEND_SAVED << 8;
}

static void analoginOscHandler(OscChannel ch, char* address, int idx, OscData d[], int datalen)
{
  UNUSED(d); UNUSED(address);
  if (datalen == 0) {
    OscData d = { .type = INT, .value.i = analoginValue(idx) };
    oscCreateMessage(ch, address, &d, 1);
  }
}

static void analoginOscAutosender(OscChannel ch)
{
  uint8_t i;
  OscData d = { .type = INT };
  char addr[19];
  for (i = 0; i < ANALOGIN_CHANNELS; i++) {
    if (analoginAutosendChannels & (1 << i)) {
      d.value.i = analoginValue(i);
      if (analoginAutosendVals[i] != d.value.i) {
        analoginAutosendVals[i] = d.value.i;
        sniprintf(addr, sizeof(addr), "/analogin/%d/value", i);
        oscCreateMessage(ch, addr, &d, 1);
      }
    }
  }
}

static void analoginAutosendHandler(OscChannel ch, char* address, int idx, OscData d[], int datalen)
{
  UNUSED(d);
  UNUSED(address);
  if (datalen == 0) {
    OscData d;
    d.type = INT;
    d.value.i = (analoginAutosendChannels & (1 << idx)) ? 1 : 0;
    oscCreateMessage(ch, address, &d, 1);
  }
  else if (datalen == 1) {
    if (d[0].value.i)
      analoginAutosendChannels |= (1 << idx);
    else
      analoginAutosendChannels &= ~(1 << idx);

    eepromWrite(EEPROM_ANALOGIN_AUTOSEND, analoginAutosendChannels);
  }
}

static const OscNode analoginAutosendNode = { .name = "autosend", .handler = analoginAutosendHandler };
static const OscNode analoginValueNode = { .name = "value", .handler = analoginOscHandler };

const OscNode analoginOsc = {
  .name = "analogin",
  .range = ANALOGIN_CHANNELS,
  .children = { &analoginValueNode, &analoginAutosendNode, 0 },
  .autosender = analoginOscAutosender
};
#endif // OSC
