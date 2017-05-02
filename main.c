
/* B Knudsen Cc5x C-compiler - not ANSI-C */
#include "16F690.h"
#pragma config |= 0x00D4

//Declare functions
void initserial(void);
void putchar(char);
void printf(const char *string, char variable);
void unsLong_out(unsigned long number);
void delay10(char);

//Delays program by specified uS ammount
void delayUS(unsigned long us)
{
	//Reset timers
	TMR1H = 0;
	TMR1L = 0;

	unsigned long elapsed = 0;

	//Wait for time to elapse
	while (elapsed < us)
	{
		elapsed = TMR1H * 256;
		elapsed += TMR1L;
	}

	//Return to mainloop
	return;
}

void main(void)
{
	initserial();

	//Initialize PORTC.0 (RC0) as output
	TRISC.0 = 0;

	//Initialize PORTA.3 (RA3) as input
	TRISA.3 = 1;

	//SET ALL PORTB'S TO DIGITAL
	ANSELH = 0;

	//Initialize TRISB.4 (RB4) as output
	TRISB.4 = 0;

	//Initialize TRISB.5 (RB5) as input
	TRISB.5 = 1; /* ECHO -input */


	/* Initialize TIMER1 */
	/*
	00.xx.x.x.x.x  --
	xx.00.x.x.x.x  Prescale 1/1
	xx.xx.0.x.x.x  TMR1-oscillator is shut off
	xx.xx.x.0.x.x  - (clock input synchronization)
	xx.xx.x.x.0.x  Use internal clock f_osc/4
	xx.xx.x.x.x.1  TIMER1 is ON
	*/
	T1CON = 0b00.00.0.0.0.1;

	//Initialize togglevariable to false
	bit measure = 0;

	//Infinite-loop
	while (1)
	{
		/* Prevent an instant re-measure (60ms per specification) */
		delay10(6);

		//Wait for user to start measuring sequence
		while (measure == 0)
		{
			while (PORTA.3 == 1);
			delay10(1);
			measure = 1;
			while (PORTA.3 == 0);
		}

		//Indicate that we're measuring
		PORTC.0 = 1;

		//Send trigger to ultrasonic sensor
		PORTB.4 = 1;

		// Keep the trigger active for (10 uS per specification)
		delayUS(10); 

		// Disable the trigger
		PORTB.4 = 0;

		//Wait for positive edge
		while (PORTB.5 == 0);

		//Reset TIMER1
		TMR1H = 0;
		TMR1L = 0;

		//Wait for negative edge
		while (PORTB.5 == 1);

		//Load the value of TIMER1 into 'elapsed' variable
		unsigned long elapsed = 0;
		elapsed = TMR1H * 256;
		elapsed += TMR1L;

		//Calculate centemeters measured
		unsigned long centimeters = elapsed / 58;

		//Indicate that measuring is done
		PORTC.0 = 0;

		//Print distance
		printf("%ucm\r\n", (char)centimeters);

		//If we press button, stop measuring
		if (PORTA.3 == 0)
		{
			while (PORTA.3 == 0);
			delay10(1);
			measure = 0;
		}
	}
}


/* *********************************** */
/*            FUNCTIONS                */
/* *********************************** */

/* **** bitbanging serial communication **** */

void initserial(void)  /* initialise PIC16F690 bbCom */
{
	ANSEL.0 = 0; // No AD on RA0
	ANSEL.1 = 0; // No AD on RA1
	PORTA.0 = 1; // marking line
	TRISA.0 = 0; // output to PK2 UART-tool
	TRISA.1 = 1; // input from PK2 UART-tool - not used
	return;
}

void putchar(char ch)  // sends one char bitbanging
{
	char bitCount, ti;
	PORTA.0 = 0; // set startbit
	for (bitCount = 10; bitCount > 0; bitCount--)
	{
		// delay one bit 104 usec at 4 MHz
		// 5+18*5-1+1+9=104 without optimization 
		ti = 18; do; while (--ti > 0); nop();
		Carry = 1;     // stopbit
		ch = rr(ch); // Rotate Right through Carry
		PORTA.0 = Carry;
	}
	return;
}

void printf(const char *string, char variable)
{
	char i, k, m, a, b;
	for (i = 0;; i++)
	{
		k = string[i];
		if (k == '\0') break;   // at end of string
		if (k == '%')           // insert variable in string
		{
			i++;
			k = string[i];
			switch (k)
			{
			case 'd':         // %d  signed 8bit
				if (variable.7 == 1) putchar('-');
				else putchar(' ');
				if (variable > 127) variable = -variable;  // no break!
			case 'u':         // %u unsigned 8bit
				a = variable / 100;
				putchar('0' + a); // print 100's
				b = variable % 100;
				a = b / 10;
				putchar('0' + a); // print 10's
				a = b % 10;
				putchar('0' + a); // print 1's
				break;
			case 'b':         // %b BINARY 8bit
				for (m = 0; m < 8; m++)
				{
					if (variable.7 == 1) putchar('1');
					else putchar('0');
					variable = rl(variable);
				}
				break;
			case 'c':         // %c  'char'
				putchar(variable);
				break;
			case '%':
				putchar('%');
				break;
			default:          // not implemented
				putchar('!');
			}
		}
		else putchar(k);
	}
}

/* ***************************************** */


/* **** delay function **** */

void delay10(char n)
{
	char i;
	OPTION = 7;
	do  {
		i = TMR0 + 39; /* 256 microsec * 39 = 10 ms */
		while (i != TMR0);
	} while (--n > 0);
}

/* *********************************** */
/*            HARDWARE                 */
/* *********************************** */

/*
		_____________  _____________
		|             \/             |
  +5V---|Vdd        16F690        Vss|---Gnd
		|RA5            RA0/AN0/(PGD)|
		|RA4/AN3            RA1/(PGC)|
  BTN0<-|RA3/!MCLR/(Vpp)  RA2/AN2/INT|
		|RC5/CCP                  RC0|->-LED0
		|RC4                      RC1|
		|RC3/AN7                  RC2|
		|RC6/AN8             AN10/RB4|-> HC-SR04 -> TRIGGER
		|RC7/AN9               RB5/Rx|-> HC-SR04 -> ECHO
		|RB7/Tx                   RB6|
		|____________________________|
*/
