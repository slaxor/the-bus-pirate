/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Initial written by Chris van Dongen, 2010.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/*

these are lowlevel interface subs for programming pics

supported platform (should) are:

pic10, pic12, pic14, pic16 and pic18

{0xFF} sends 4 byte cmd (pic18)
[0xFF] sends 6 byte cmd (pic10, 12, 14, 16)

*/

#include "pic.h"
#include "bitbang.h"
#include "baseIO.h"
#include "base.h"
#include "buspiratecore.h"
#include "procmenu.h"		// for the userinteraction subs
#include "AUXpin.h"

extern struct _bpConfig bpConfig;
extern struct _modeConfig modeConfig;

#ifdef BP_USE_PIC

int picmode;
int piccmddelay;

void picinit(void)
{	int mode, delay;
	int interactive;

	consumewhitechars();
	mode=getint();
	consumewhitechars();
	delay=getint();
	interactive=0;

	if(!((mode>0)&&(mode<=2)))
	{	interactive=1;
	}

	if((delay>0)&&(delay<=2))
	{	piccmddelay=delay;
	}
	else
	{	interactive=1;
	}

	if(interactive)
	{	cmderror=0;

		bpWline("Commandmode");
		bpWline("1. 6b/14b");
		bpWline("2. 4b/16b");
	
		mode=getnumber(1,2,0); 

		bpWline("Delay");
		delay=getnumber(1,2,0);
	}

	switch(mode)
	{	case 1:	picmode=PICMODE6;
				break;
		case 2: picmode=PICMODE4;
				break;
		default: break;
	}
	piccmddelay=delay;

	if(!interactive)
	{	bpWstring("PIC(mod dly)=(");
		bpWdec(picmode); bpSP;
		bpWdec(piccmddelay);
		bpWline(")");
	}

	modeConfig.HiZ=1;				// to allow different Vcc 
	modeConfig.allowpullup=1;		// pullup is allowed
	modeConfig.int16=1;
	bbL(MOSI|CLK, PICSPEED);		// pull both pins to 0 before applying Vcc and Vpp
}

void piccleanup(void)
{	modeConfig.int16=0;				// other things are cleared except this one :D (we introduced it :D)
}

void picstart(void)					// switch  to commandmode
{	picmode|=PICCMD;
	bpWstring("CMD");
	UART1TX(0x30+(picmode&PICMODEMSK));			// display #commandbits 
	modeConfig.int16=0;
	bpSP;
}

void picstop(void)					// switch to datamode
{	picmode&=PICMODEMSK;
	modeConfig.int16=1;				// data is 14-16 bit
	bpWline("DTA");
}

unsigned int picread(void)
{	int i;
	unsigned int c;

	if(picmode&PICCMDMSK)
	{	bpWline("no read");
		return 0;
	}

	c=0;

	switch(picmode&PICMODEMSK)		// make it future proof
	{	case PICMODE6:	bbR(MOSI);		// switch in to input
						bbH(CLK, PICSPEED/2);
						bbL(CLK, PICSPEED/2);
						for(i=0; i<14; i++)
						{	c>>=1;
							bbH(CLK, PICSPEED/2);
							if(bbR(MOSI)) c|=0x2000;		// bit14
							bbL(CLK, PICSPEED/2);
						}
						bbH(CLK, PICSPEED/2);
						bbL(CLK, PICSPEED/2);
						bbL(MOSI, PICSPEED/5);
						break;
		case PICMODE4:	bbR(MOSI);
						for(i=0; i<16; i++)
						{	c>>=1;
							bbH(CLK, PICSPEED/2);
							if(bbR(MOSI)) c|=0x8000;		// bit16
							bbL(CLK, PICSPEED/2);
						}
						bbH(CLK, PICSPEED/2);
						bbL(CLK, PICSPEED/2);
						bbL(MOSI, PICSPEED/5);
						break;
		default:		bpWline("unknown");
						return 0;
	}

//	bpWbin(c>>8); bpSP;
//	bpWbin(c&0x0FF); bpSP;
	return c;
}

unsigned int picwrite(unsigned int c)
{	int i;
	int mask;

	mask=0x01;

	if(picmode&PICCMDMSK)				// we got a command
	{	switch(picmode&PICMODEMSK)		// make it future proof
		{	case PICMODE6:	for(i=0; i<6; i++)
							{	bbH(CLK, PICSPEED/4);
								if(c&mask)
								{	bbH(MOSI, PICSPEED/4);
								}
								else
								{	bbL(MOSI, PICSPEED/4);
								}
								bbL(CLK, PICSPEED/4);
								bbL(MOSI, PICSPEED/4);		// both dat and clk low 
								mask<<=1;
							}
							break;
			case PICMODE4:	for(i=0; i<4; i++)
							{	bbH(CLK, PICSPEED/4);
								if(c&mask)
								{	bbH(MOSI, PICSPEED/4);
								}
								else
								{	bbL(MOSI, PICSPEED/4);
								}
								bbL(CLK, PICSPEED/4);
								bbL(MOSI, PICSPEED/4);		// both dat and clk low 
								mask<<=1;
							}
							break;
			default:		bpWline("unknown");
							return 0;
		}
		bpDelayMS(piccmddelay);
	}
	else									// send data
	{	switch(picmode&PICMODEMSK)		// make it future proof
		{	case PICMODE6:	bbH(CLK, PICSPEED/4);			// send leading 0
							bbL(MOSI, PICSPEED/4);
							bbL(CLK, PICSPEED/4);
							bbL(CLK, PICSPEED/4);
							for(i=0; i<14; i++)				// 14 bits
							{	bbH(CLK, PICSPEED/4);
								if(c&mask)
								{	bbH(MOSI, PICSPEED/4);
								}
								else
								{	bbL(MOSI, PICSPEED/4);
								}
								bbL(CLK, PICSPEED/4);
								bbL(MOSI, PICSPEED/4);		// both dat and clk low 
								mask<<=1;
							}
							bbH(CLK, PICSPEED/4);			// send trailing 0
							bbL(MOSI, PICSPEED/4);
							bbL(CLK, PICSPEED/4);
							bbL(CLK, PICSPEED/4);
							break;
			case PICMODE4:	for(i=0; i<16; i++)				// does 16 bits at a time
							{	bbH(CLK, PICSPEED/4);
								if(c&mask)
								{	bbH(MOSI, PICSPEED/4);
								}
								else
								{	bbL(MOSI, PICSPEED/4);
								}
								bbL(CLK, PICSPEED/4);
								bbL(MOSI, PICSPEED/4);		// both dat and clk low 
								mask<<=1;
							}
							break;
			default:		bpWline("unknown");
							return 0;
		}
	}
	return 0x100; 	// no data to display 
}

/*
0000 0000	return to main
0000 0001	id=PIC1
0000 0010	6b cmd
0000 0011	4b cmd
0000 01xx	xx ms delay

0000 1xxx	unimplemented

0001 0xyz	PWM|VREG|PULLUP
0001 1xyz   AUX|MISO|CS

01xx xxxx	just send cmd xxxxxxx
10xx xxxx	send cmd xxxxxxx and next two bytes (14/16 bits)
11xx xxxx	send cmd xxxxxxx and read two bytes

*/

void binpic(void)
{	unsigned char cmd;
	int ok;
	unsigned int temp;

	bpWstring("PIC1");
	modeConfig.HiZ=1;				// to allow different Vcc 
	modeConfig.allowpullup=1;		// pullup is allowed
	bbL(MOSI|CLK, PICSPEED);		// pull both pins to 0 before applying Vcc and Vpp
	picmode=PICMODE6;
	piccmddelay=2;

	while(1)
	{	cmd=UART1RX();

		switch(cmd&0xC0)
		{	case 0x00:	ok=1;
						switch(cmd&0xF0)
						{	case 0x00:	switch(cmd)
										{	case 0x00:	return;
											case 0x01:	bpWstring("PIC1");
														break;
											case 0x02:	picmode=PICMODE6;
														break;
											case 0x03:	picmode=PICMODE4;
														break;
											case 0x04:
											case 0x05:
											case 0x06:
											case 0x07:	piccmddelay=(cmd-0x04);
														break;
											default:	ok=0;
										}
										break;
							case 0x10:	if(cmd&0x08)
										{	if(cmd&0x04)
											{	bbH(AUX ,5);
											}
											else
											{	bbL(AUX ,5);
											}
											if(cmd&0x02)
											{	bbH(MISO ,5);
											}
											else
											{	bbL(MISO ,5);
											}
											if(cmd&0x01)
											{	bbH(CS ,5);
											}
											else
											{	bbL(CS ,5);
											}
										}
										else
										{	if(cmd&0x04)	// pwm?
											{	PWMfreq=100;
												PWMduty=50;
												updatePWM();
											}
											else
											{	PWMfreq=0;
												updatePWM();
											}
											if(cmd&0x02)	// vreg on
											{	BP_VREG_ON();
												modeConfig.vregEN=1;
											}
											else
											{	BP_VREG_OFF();
												modeConfig.vregEN=0;
											}
											if(cmd&0x01)	// pullup on
#if defined( BUSPIRATEV0A) || defined( BUSPIRATEV2)
											{	BP_PULLUP_ON(); 
												modeConfig.pullupEN=1;
											}
											else
											{	BP_PULLUP_OFF();
												modeConfig.pullupEN=0;
											}
#endif
										}
										break;
							default:	ok=0;
						}
						if(ok)
						{	UART1TX(1);
						}
						else
						{	UART1TX(0);
						}
						break;
			case 0x40:	picmode|=PICCMD;
						picwrite(cmd&0x3F);
						picmode&=PICMODEMSK;
						UART1TX(1);
						break;
			case 0x80:	picmode|=PICCMD;
						picwrite(cmd&0x3F);
						picmode&=PICMODEMSK;
						temp=UART1RX();
						temp<<=8;
						temp|=UART1RX();
						picwrite(temp);
						UART1TX(1);
						break;
			case 0xC0:	picmode|=PICCMD;
						picwrite(cmd&0x3F);
						picmode&=PICMODEMSK;
						UART1TX(1);
						temp=picread();
						UART1TX(temp>>8);
						UART1TX(temp&0x0FF);
						break;
		}
	}
}		
#endif