/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the VirtualSerial demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "VirtualSerial.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;


unsigned int deadTime;
unsigned int accumulate;
unsigned int resolution;

char cdcTxByte[255];
//char * cdcTxPointer = cdcTxByte;
	
int main(void)
{
	SetupHardware();
	DDRD |= (1 << 1); // led
	DDRD |= (1 << 0); // CS - ADC
	
	//SPI
	DDRD |= (0 << 1); // SS
	DDRB |= (2 << 1); // MOSI
	DDRB |= (1 << 1); // SCK
	
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1); // master, CLK/64
	SPSR; 
	SPDR;
	
	SS_ADC_high();
	
	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	GlobalInterruptEnable();
	
	char cdcRecByte[255];
	char * cdcRecPointer = cdcRecByte;
	
	char * substr = malloc(5);

	
	for (;;)
	{
		int16_t rec = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		if(rec > 0){
			* cdcRecPointer = (char)rec;
			cdcRecPointer++;
			* cdcRecPointer = 0; // zapis konca transmisji

			if(rec == 'A'){
				cdcRecPointer =& cdcRecByte[0];
			}
			if(rec == 'B'){
				//fputs(cdcRecByte, &USBSerialStream);
				strncpy(substr, cdcRecByte, 4);
				deadTime = atoi(substr);
				strncpy(substr, cdcRecByte+4, 4);
				resolution = atoi(substr);
				strncpy(substr, cdcRecByte+8, 4);
				accumulate = atoi(substr);
				
				doMeasurement();
				
				//sprintf(cdcTxByte, "%04u-%04u-%04u", deadTime, resolution, accumulate);

				//fputs(cdcTxByte, &USBSerialStream);
				cdcRecPointer =& cdcRecByte[0];
			}
		}
			//unsigned int value = getADCdata(0);
			
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

void doMeasurement(void){
	// ustaw napiecie na 0
	for (int i = 0; i < (65536 / resolution) ; i++) // rozdzielczosc nie jest stopniem a iloscia krokow
	{
		// ustaw napiecie ++ w zaleznosci od resolution
		// _delay(deadTime);
		unsigned int ch0 = 0;
		unsigned int ch1 = 0;
		unsigned int ch2 = 0;
		unsigned int ch3 = 0;
		
		for (int i = 0; i < accumulate ; i++)
		{
			ch0 += getADCdata(0); // dac tutaj jakies delay?
			ch1 += getADCdata(1);
			ch2 += getADCdata(2);
			ch3 += getADCdata(3);
		}
		
		ch0 /= accumulate;
		ch1 /= accumulate;
		ch2 /= accumulate;
		ch3 /= accumulate;
		
		sprintf(cdcTxByte, "Y%05u-%05u-%05u-%05uX", ch0, ch1, ch2, ch3);
		fputs(cdcTxByte, &USBSerialStream);
	}
}

void SS_ADC_high(void){ //PD0
	PORTD |= (1 << 0); // PD0 goes high
}

void SS_ADC_low(void){ //PD0
	PORTD &= ~(1 << 0); // PD0 goes low
}

uint8_t SPI_send_rec_byte(uint8_t byte){ 
	SPDR=byte; 
	while(!(SPSR & (1 << SPIF)));
	return SPDR;
}

unsigned int getADCdata(unsigned int channel){ // TODO: ustawic czanel
		char data[2];
		SS_ADC_low();
	    SPDR = 0x06;
	    while(!(SPSR & (1<<SPIF)));
	    data[0] = SPDR; // data H
	    SPDR = channel<<6;
	    while(!(SPSR & (1<<SPIF)));
	    data[0] = SPDR; // MSB H data
	    SPDR = 0x00;
	    while(!(SPSR & (1<<SPIF)));
	    data[1] = SPDR; // LSB L data
		SS_ADC_high();
		data[0] = data[0] & 0b00001111;
		return data[0] * 256 + data[1];
}

void blink(void){
	PORTD ^= (1 << 1);
	_delay_ms(200);
	PORTD ^= (1 << 1);
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#endif

	/* Hardware Initialization */
	Joystick_Init();
	LEDs_Init();
	USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

