#include "keithleyDisplay.h"
#include <stdio.h>
#include <stdint.h>

#if defined(PICO_RP2040)
#  include "pico/stdlib.h"
#endif

#ifndef uint8_t
  typedef unsigned char uint8_t;
#endif

extern int iDebugBits;			// for debugging so we can log different kind of events (defined in main.cpp)

// the K2000 signal levesl are 3.3V
// the K2001 signal levels are 5V

// key-press on the front-panel of the K2000 display
/*  K2000 key from display to CPU
 *    Shift  0x41 'A'   0x40 '@'
 *    DCV    0x42 'B'   0x40 '@'
 *    ACV    0x43 'C'   0x40 '@'
 *    DCI    0x44 'D'   0x40 '@'
 *    ACI	 0x45 'E'   0x40 '@'
 *    Ohm2   0x46 'F'   0x40 '@'
 *    Ohm4   0x47 'G'   0x40 '@'
 *    Freq   0x48 'H'   0x40 '@'
 *    Up^    0x4B 'K'   0x40 '@'
 *    Auto   0x4C 'L'   0x40 '@'
 *    Down   0x4D 'M'   0x40 '@'
 *    Enter  0x4E 'N'   0x40 '@'
 *    Ritht> 0x4F 'O'   0x40 '@'
 *    Temp   0x50 'P'   0x40 '@'
 *    Local  0x51 'Q'   0x40 '@'
 *    ExtTrg 0x52 'R'   0x40 '@'
 *    Trig   0x53 'S'   0x40 '@'
 *    Store  0x54 'T'   0x40 '@'
 *    Recall 0x55 'U'   0x40 '@'
 *    Filter 0x56 'V'   0x40 '@'
 *    Rel	 0x57 'W'   0x40 '@'
 *    Left<  0x58 'X'   0x40 '@'
 *    Open   0x5A 'Z'   0x40 '@'
 *    Colse  0x5B '['   0x40 '@'
 *    Step   0x5C '\'	0x40 '@'
 *    Scan   0x5D ']'   0x40 '@'
 *    Digits 0x5E '^'   0x40 '@'
 *    Rate   0x5F '_'	0x40 '@'
 *    Exit   0x60 '`'   0x40 '@'
*/

 /*   K2001  key from display to CPU
 *    Key    Press      Release
 *    Up ^   0x41 'A'   0x40 '@'
 *    Temp   0x42 'B'   0x40 '@'
 *    Left<  0x43 'C'	0x40 '@'
 *    Menu   0x44 'D'	0x40 '@'
 *    ACI    0x45 'E'   0x40 '@'
 *    Store  0x46 'F'	0x40 '@'
 *    Local  0x47 'G'	0x40 '@'
 *  D Prev   0c48 'H'   0x40 '@'
 *    Auto   0x49 'I'	0x40 '@'
 *    Right> 0x4A 'J'	0x40 '@'
 *    Ext    0x4B 'K'	0x40 '@'
 *    Ohm2   0x4C 'L'   0x40 '@'
 *    Recall 0x4D 'M'	0x40 '@'
 *    Chan   0x4E 'N	0x40 '@'
 *    DCV    0x4F 'O'   0x40 '@'
 *  D Next   0x50 'P'   0x40 '@'
 *    Down v 0x51 'Q'	0x40 '@'
 *    Enter  0x52 'R'	0x40 '@'
 *    Ohm4   0x53 'S'   0x40 '@'
 *    Filter 0x54 'T'	0x40 '@'
 *    Scan   0x55 'U'	0x40 '@'
 *    ACV    0x56 'V'   0x40 '@'
 *    Rel    0x57 'W'   0x40 '@'
 *           0x58 'X'   0x40 '@'
 *           0x59 'Y'   0x40 '@'
 *    Freq   0x5A 'Z'   0x40 '@'
 *    Math   0x5B '['	0x40 '@'
 *    Config 0x5C '\'	0x40 '@'
 *    DCI    0x5D ']'   0x40 '@'
 *    TRG    0x5E '^'   0x40 '@'
 *    Info   0x5F '_'	0x40 '@'
 */


keithleyDisplay::keithleyDisplay()
{
	initVars();
}

#ifdef _WIN32
keithleyDisplay::keithleyDisplay(std::vector<uint32_t>& simData)
{
	initVars();
	con.setSimulateData(simData);

}
keithleyDisplay::keithleyDisplay(const char* sPortName)
{
	initVars();
	con.connect(sPortName,9600, 'n', 8, 1);
}
#endif

#if defined(PICO_RP2040)
keithleyDisplay::keithleyDisplay(uart_inst_t *pUart)
{
	initVars();
    con.connect(pUart);
}
#endif

keithleyDisplay::~keithleyDisplay()
{
}


void keithleyDisplay::poll()
{
	const uint32_t msgTimeoutMS = 50u;

	switch (iDeviceType)
	{
	case 2000:
		bSingleByteOptions = true;
		break;
	case 2001:
		bSingleByteOptions = false;
		break;
	}

	while (true)
	{
		int ch = con.getc(0);
		uint32_t diffMS = stlMsTimer(lastReceivedMS);

		// if the first char we receive after startup is B7 then we have a K2001
		if ((ch == 0xB7) && (ringHead == 0) && (iDeviceType == 0))
		{
			iDeviceType = 2001;
			bSingleByteOptions = false;		// K2001 has 2 byte options for the 06 and 07 commands otherwise we have a K2000 with single byte options
		}

		// part used in simulation of saved data
		if ((ch > 0) && (ch & 0x7FFF0000)) {		// simulation bit 16-32 contains time with last character
			diffMS = ch >> 16;
			ch &= 0xFFFF;
			if ((diffMS > msgTimeoutMS) && (iCurMsgType)) { handleCompleteMessage(); iCurMsgType = 0; }
		}
		// end simulation data

		if (ch < 0)		// no more data in the fifo see if we can start updating the screen
		{
			// if we have no data for 5ms the commit the current buffer
			if ((iCurMsgType) && (diffMS > msgTimeoutMS))		// after 50 ms of silence assume the message is complete
			{
				handleCompleteMessage();
				iCurMsgType = 0;
			}
			break;
		}
		
		// for debugging so we can dump the last 4k bytes 
		lastReceivedMS += diffMS;
		ringBuffer[ringHead] = ch;
		ringBufferMS[ringHead] = lastReceivedMS;
		ringHead = (ringHead + 1) % _RingSize_;
		
		//if (iDoubleByteCmd == 0x09){
		//	if (ch == 0) continue;		// ignore the 0 char after 09 command reason unknown but we can ignore it
		//	iDoubleByteCmd = 0;
		//}
	
		// detect if we have start of new data message
		bool bStartNext = ((ch <= 0x0F) && (iReadLeng == 0) && (iDoubleByteCmd == 0));
		if ((iReadLeng > 0) && (msg.length() >= iReadLeng)) bStartNext = true;

		// detect if the start is a double byte command message
		if ((iDoubleByteCmd == 0) && (bStartNext))
		{
			// on k2000 the following commands of the options
			// 02     single byte command what does this do ???
			// 06 xx  the 2001 does 06 xx xx
			// 07 xx  the 2001 does 07 xx xx
			// 08 xx
			// 09 xx
			// 0A xx
			// 0E xx 
			//  6  7  8  9  A  E
			// 00 10 00 00 00 00 rear
			// 00 80 00 00 00 00 shift
			// 00 01 00 00 00 00 <>
			// 00 02 00 00 00 00 4w
			// 00 04 00 00 00 00 .>) beep
			// 00 08 00 00 00 00 ->|- diode
			// 00 00 01 00 00 00 slow 
			// 00 00 02 00 00 00 med
			// 00 00 04 00 00 00 fast
			// 00 00 08 00 00 00 trig
			// 00 00 00 10 00 00 auto
			// 00 00 00 20 00 00 flt   
			// 00 00 00 40 00 00 rel
			// 00 00 00 04 00 00 *
			// 00 00 00 00 08 00 step

			if (iDebugBits & 8)
				printf("iDoubleByteCmd start 0x%X\r\n",ch);

			if (ch == 0x2)
			{
				if (iCurMsgType) handleCompleteMessage();
				//msg.set(msgLatest);	// do we need this ?
				continue;
			}

			if (ch != 0xF)
			//if ((ch == 0x6) || (ch == 0x7) || (ch == 0x8) || (ch == 0x9) || (ch == 0xA) || (ch == 0xE)) 
			{
				if ((iCurMsgType) && (ch != 0x0B)) handleCompleteMessage();
				if (bSingleByteOptions){ iDoubleByteCmd = ch; continue; } 
				if ((ch == 0x6) || (ch == 0x7)) { iReadLeng = 2; iCurMsgType = ch; } else	iDoubleByteCmd = ch;
			}
				
			// For k2001 the responce is as below
			// if (ch == 0x0F) { con.writef("200x/700x A02  "); con.write("\x80\x00",2); }
			// for K2000 the responce is as below
			// if (ch == 0x0F) { con.writef("2000 A02  "); con.write("\x80\x00",2); }
			continue;
		}

		if (iDoubleByteCmd)
		{
			if (iDoubleByteCmd == 0x0B) { flashChars = (ch != 0); iDoubleByteCmd = 0; continue; }		// start stop flashing of the ASCII chars
			handleCompleteMessage();
			iCurMsgType = iDoubleByteCmd | (ch << 8);
			iDoubleByteCmd = 0;
			// if we start at a offset get last complete message from memory
			uint8_t iCmd    = iCurMsgType & 0xFF;
			uint8_t iCmdPar = iCurMsgType >> 8;

			if ((iCmd == 0x6) || (iCmd == 0x7) || (iCmd == 0x8) || (iCmd == 0x9) || (iCmd == 0xA) || (iCmd == 0xE))
			{
				k2000bits[iCmd & 0xF] = iCmdPar;
				if (!k2000bits.compare(k2000bitsOld)) {
					k2000bitsOld.set(k2000bits);
					printf("Option tp %02X -- %02X %02X %02X %02X %02X %02X\r\n", iCmd, k2000bits[6] & 0xFF, k2000bits[7] & 0xFF, k2000bits[8] & 0xFF, k2000bits[9] & 0xFF, k2000bits[10] & 0xFF, k2000bits[14] & 0xFF);
					printf("%s\r\n", getDispHeadTxts().buf());
					bIsUpdated = true;
				}
				iCurMsgType = 0;
				continue;
			}

			// on the K2000 the message is usually preceeded by a single 0D folowed byt ye messaga in ASCII
			// in some cases 0xD is used a <CR> and we overwrite the current line starting at position 0
			if ((iCmd == 0x0D)||(iCmd == 0x04)||(iCmd == 0x03))
			{
				if (iCmdPar < 0x20)		// offset in the last message
				{
					if (iCmdPar > 0) msg.set(msgLatest); else msg.clear();
					iMsgDupOffset = iCmdPar;
					bCurMsgFlash = bLastMsgFlash;
				}else{
					msg.clear();
					msg.append((char)iCmdPar);
					iMsgDupOffset = 0;
				}
				iCurMsgType = 0x04;
			}
			continue;
		}

		// iDoubleByteCmd sequences
		// 0B 01  start flashing text  0B 00 stop flashing text
		// 0D 03  (normal 04 string but keep buffer in tact)
		// 04 04  overwrite previous message at position 4
		// 04 00  start text line

		if (bStartNext)						// start of new message not type 04 0D 0B
		{
			if (iCurMsgType) handleCompleteMessage();
			iCurMsgType = ch;
			if ((iCurMsgType == 6) || (iCurMsgType == 7)) iReadLeng = 2;
			//if (iReadLeng != 0) printf("msg %d len %d opt %d\r\n", iCurMsgType, iReadLeng,bSingleByteOptions);
			continue;
		}
		if ((iCurMsgType == 0) && (ch >= 0x20) && (ch <= 0xB0))
			iCurMsgType = 0x04;

		if (iCurMsgType) 
		{
			if ((flashChars) && ((iCurMsgType & 0xFF) == 04)){ ch |= 0x80; bCurMsgFlash = true; }

			if (iMsgDupOffset)	// we need to overwrite the old message
			{
				while (iMsgDupOffset >= msg.length()) msg.append(' ');
				msg[iMsgDupOffset++] = (char)ch;
			}
			else
				msg.append((char)ch);
			bUnExpected = false;
		}
		else
		{
			if (bValidCom)
			{
				if (!bUnExpected)
				{
					bUnExpected = true;
					printf("Unexpected char 0x%02X ", ch);
				}else{
					printf("0x%02X ", ch);
				}
			}
		}
	}
}

void appendDescrK2000(ansStl::cST& res,uint8_t val,const char *t7,const char *t6,const char *t5,const char *t4,const char *t3,const char *t2,const char *t1,const char *t0)
{
	if (val == 0) return;
	if (val & 0x01) { res.append(t0); res.append((char)' '); }
	if (val & 0x02) { res.append(t1); res.append((char)' '); }
	if (val & 0x04) { res.append(t2); res.append((char)' '); }
	if (val & 0x08) { res.append(t3); res.append((char)' '); }
	if (val & 0x10) { res.append(t4); res.append((char)' '); }
	if (val & 0x20) { res.append(t5); res.append((char)' '); }
	if (val & 0x40) { res.append(t6); res.append((char)' '); }
	if (val & 0x80) { res.append(t7); res.append((char)' '); }
}

ansStl::cST keithleyDisplay::getDispHeadTxts()
{
	ansStl::cST res;
	if (bSingleByteOptions)
	{
		appendDescrK2000(res,k2000bits[0x6],"-67-","-66-","-65-","-64-","-63-","-62-","-61-","-60-");
		appendDescrK2000(res,k2000bits[0x7],"SHIFT","-76-","-75-","REAR","->|-",".>)}","4w","v-^");
		appendDescrK2000(res,k2000bits[0x8],"-87-","-86-","-85-","-84-","TRIG","FAST","MEDIUM","SLOW");
		appendDescrK2000(res,k2000bits[0x9],"-97-","REL","FILT","AUTO","-93-","*","-91-","-90-");
		appendDescrK2000(res,k2000bits[0xA],"-A7-","-A6-","-A5-","-A4-","STEP","-A2-","-A1-","-A0-");
		appendDescrK2000(res,k2000bits[0xE],"-E7-","-E6-","-E5-","-E4-","-E3-","-E2-","-E1-","-E0-");
		return res;
		//  6  7  8  9  A  E
		// 00 10 00 00 00 00 rear
		// 00 80 00 00 00 00 shift
		// 00 01 00 00 00 00 <>
		// 00 02 00 00 00 00 4w
		// 00 04 00 00 00 00 .>) beep
		// 00 08 00 00 00 00 ->|- diode

		// 00 00 01 00 00 00 slow 
		// 00 00 02 00 00 00 med
		// 00 00 04 00 00 00 fast
		// 00 00 08 00 00 00 trig
		// 00 00 00 10 00 00 auto
		// 00 00 00 20 00 00 flt   
		// 00 00 00 40 00 00 rel
		// 00 00 00 04 00 00 *
		// 00 00 00 00 08 00 step
	}


	if (msg67bits & 0x0100) res.append("EDIT ");
	if (msg67bits & 0x0200) res.append("ERR ");
	if (msg67bits & 0x0400) res.append("REM ");
	if (msg67bits & 0x0800) res.append("TALK ");
	if (msg67bits & 0x1000) res.append("LSTN ");
	if (msg67bits & 0x2000) res.append("SRQ ");
	if (msg67bits & 0x4000) res.append("REAR ");
	if (msg67bits & 0x8000) res.append("REL ");
	if (msg67bits & 0x0001) res.append("FILT ");
	if (msg67bits & 0x0002) res.append("MATH ");
	if (msg67bits & 0x0004) res.append("4W ");
	if (msg67bits & 0x0008) res.append("AUTO ");
	if (msg67bits & 0x0010) res.append("ARM ");
	if (msg67bits & 0x0020) res.append("TRIG ");
	if (msg67bits & 0x0040) res.append("* ");
	if (msg67bits & 0x0080) res.append("SMPL ");
	return res;// "EDIT ERR REM TALK LSTN SRQ REAR REL FILT MATH 4W AUTO ARM TRIG * SMPL"
}

void showDisplay(ansStl::cST msg, ansStl::cST& msLst,int iDebugBits)
{
    #if defined(PICO_RP2040)
	for (int i = 0; i < msg.length(); i++) if ((((uint8_t)msg[i]) >= 0x7F) || (msg[i] < 32)) msg[i] = '.';
	#else
	for (int i = 0; i < msg.length(); i++) if (((uint8_t)msg[i]) > 0xB0) msg[i] &= 0x7F;
	#endif
	if (iDebugBits & 2)
		printf("-%2d-%2d- ", msLst.length(), msg.length());
	if (iDebugBits & 1)
		printf("\"%s\"", msg.buf());
	printf("\r\n");
}


void keithleyDisplay::handleCompleteMessage()
{
	if (iCurMsgType == 0) return;

	//printf("handleCompleteMessage 0x%X\r\n",iCurMsgType);

	uint8_t iCmd = iCurMsgType & 0x0F;
	uint8_t iPar = (iCurMsgType >> 8) & 0xFF;
	iCurMsgType = 0;
	iReadLeng = 0;
	iMsgDupOffset = 0;

	switch (iCmd)
	{
	case 0x0D:		
		// 0D 02 and 0D 03 seen as memory dup line 
		// 0D followed by char >= 20h (space) looks like normal EOL type of commit
	case 0x04:
	{
		if (iDebugBits & 3)
		{
			if (iDebugBits & 2)
				printf("-- %02X %02X --", iCmd, iPar);
			showDisplay(msg, msgLatest,iDebugBits);
		}
		if (msg.length() > 0)
			msgLatest = msg;
		bValidCom = true;
		bIsUpdated = true;
		bLastMsgFlash = bCurMsgFlash;
		bCurMsgFlash = false;
		break;
	}
	case 6:
		/*   K2000 display seems to have a 1 byte message for the option bits
		**         06 07 08 09 0A and 0E 
		*/
		/*  K2001 display has a 2 byte message with bits for the display header text as below
		**   ** values of 06 **
		**    0x0001  FLT
		**    0x0002  MATH
		**    0x0004  4W
		**    0x0008  AUTO
		**    0x0010  ARM
		**    0x0020  TRIG
		**    0x0040  *
		**    0x0080  SMPL
		**    0x0100  EDIT
		**    0x0200  ERR
		**    0x0400  REM
		**    0x0800  TALK
		**    0x1000  LSTN
		**    0x2000  SRQ
		**    0x4000  REAR
		**    0x8000  REL
		*/

		if (msg.length() == 2) msg67bits = ((msg[0] & 0xFF) << 8) | (msg[1] & 0xFF);
		if (msg6.compare(msg)) break;		// no changes
		msg6.set(msg);
		if (msg.length() == 2)
		{
			if (bValidCom)
			{
				bIsUpdated = true;
				bLastMsgFlash = bCurMsgFlash;
				bCurMsgFlash = false;
			}
			printf("06 %02X %02X", msg[0] & 0xFF, msg[1] & 0xFF);
			break;
		}
		if (bValidCom)
			printHex(1);
		break;
	case 7:
		if (msg7.compare(msg)) break;		// no changes
		msg7.set(msg);
		if (msg.length() == 2)
		{
			bIsUpdated = true;
			bLastMsgFlash = bCurMsgFlash;
			bCurMsgFlash = false;
			//printf("07 %02X %02X", msg[0] & 0xFF, msg[1] & 0xFF);
			break;
		}
		if (bValidCom)
			printHex(2);
		break;
	default:
		if (bValidCom)
			printHex(3);
		break;
	}
	msg.clear();
	iCurMsgType = 0;
	iReadLeng = 0;
	iMsgDupOffset = 0;
}

void keithleyDisplay::saveBuffer(int iDumpLen /* = 0 */)
{
	ansStl::cST sFN;
	ansStl::cST hMSG;
	int iLeng = (_RingSize_ - 100) & 0xFFFE0;
	if ((iDumpLen > 0) && (iDumpLen < iLeng)) iLeng = iDumpLen & 0xFFFE0;
	int iStart = ringHead - iLeng;
	if (iStart < 0) iStart += _RingSize_;
	uint32_t lastMS = ringBufferMS[iStart];
	int iRowBytes = 32;

	#ifdef _WIN32
	sFN.consume(stlDateFormat("dump%H%M%S.txt"));
	FILE* f1 = fopen(sFN, "wb");
	if (f1)
	#endif
	{
		for (int iOfs = 0; iOfs < iLeng; iOfs += iRowBytes)
		{
			hMSG.setf("0x%04X - ", iOfs);
			for (int i = 0; i < iRowBytes; i++)
			{
				int idx = (iStart + iOfs + i) % _RingSize_;
				hMSG.appendf(" %02X", ringBuffer[idx] & 0xFF);
			}
			hMSG.append("  ");
			for (int i = 0; i < iRowBytes; i++)
			{
				int idx = (iStart + iOfs + i) % _RingSize_;
				int ch = ringBuffer[idx];
				if ((ch >= 32) && (ch < 127))
					hMSG.append((char)ch);
				else
					hMSG.append('.');
			}
			hMSG.append("\r\n");

			#if defined(PICO_RP2040)
			printf("%s",hMSG.buf());
			#else
			fwrite(hMSG.buf(), hMSG.length(), 1, f1);
			#endif


			// timing interval of the characters
			hMSG.set("-- int --");
			for (int i = 0; i < iRowBytes; i++)
			{
				int idx = (iStart + iOfs + i) % _RingSize_;
				uint32_t diffMS = ringBufferMS[idx] - lastMS;
				lastMS += diffMS;
				if (diffMS > 999) diffMS = 999;
				hMSG.appendf("%3d", diffMS);
			}
			hMSG.append("\r\n");

			#if defined(PICO_RP2040)
			printf("%s",hMSG.buf());
			#else
			fwrite(hMSG.buf(), hMSG.length(), 1, f1);
			#endif
		
			hMSG.clear();
		}
		#ifdef _WIN32
		fclose(f1);
		#endif
		printf("saved to %s done\r\n", sFN.buf());
	}
}


void keithleyDisplay::printHex(int iSource)
{
	#ifdef _WIN32
	ansStl::cST hMSG;
	int iLeng = 20;
	int iStart = ringHead - iLeng;

	hMSG.setf("Err: src %d cur %d 0x%04X - ", iSource, iCurMsgType, ringHead);

	for (int i = 0; i < iLeng; i++)
	{
		int idx = iStart + i;
		if (idx < 0) idx += _RingSize_;
		hMSG.append(" %02X", ringBuffer[idx] & 0xFF);
	}
	hMSG.append("  ");
	for (int i = 0; i < iLeng; i++)
	{
		int idx = iStart + i;
		if (idx < 0) idx += _RingSize_;
		int ch = ringBuffer[idx];
		if ((ch >= 32) && (ch < 127))
			hMSG.append((char)ch);
		else
			hMSG.append('.');
	}
	printf("%s", hMSG.buf());
    #endif
}

