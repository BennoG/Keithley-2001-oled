#include "keithleyDisplay.h"
#include <stdio.h>
#include <stdint.h>

#if defined(PICO_RP2040)
#  include "pico/stdlib.h"
#endif

#ifndef uint8_t
  typedef unsigned char uint8_t;
#endif

// key-press on the front-panel of the K2001 display

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
	//con.setSimulateDataHex(hexSim);
	//con.connect("COM3", 9600, 'n', 8, 1);
	//con.connect("COM5", 9600, 'n', 8, 1);
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
	while (true)
	{
		int ch = con.getc(0);
		uint32_t diffMS = stlMsTimer(lastReceivedMS);

		// part used in simulation of saved data
		if ((ch > 0) && (ch & 0x7FFF0000)) {		// simulation bit 16-32 contains time with last character
			diffMS = ch >> 16;
			ch &= 0xFFFF;
			if ((diffMS > 20u) && (iCurMsgType)) { handleCompleteMessage(); iCurMsgType = 0; }
		}
		// end simulation data

		if (ch < 0)
		{
			// if we have no data for 200ms the commit the current buffer
			if ((iCurMsgType) && (diffMS > 20u))		// after 20 ms of silence assume the message is complete
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
		
		if (iDoubleByteCmd == 0x09){
			if (ch == 0) continue;		// ignore the 0 char after 09 command reason unknown but we can ignore it
			iDoubleByteCmd = 0;
		}
		
		// detect if we have start of new data message
		bool bStartNext = ((ch <= 0x0F) && (iReadLeng == 0) && (iDoubleByteCmd == 0));
		if ((iReadLeng > 0) && (msg.length() >= iReadLeng)) bStartNext = true;

		// detect if the start is a double byte command message
		if ((iDoubleByteCmd == 0) && (bStartNext))
		{
			// 04 00   start new normal text  04 01 overwrite previous text starting at position 1
			if (ch == 0x04)	iDoubleByteCmd = ch;
			// 08 01 80 80 80
			if (ch == 0x08) iDoubleByteCmd = 0x04;		// same as 04 01 ???
			if (ch == 0x09) iDoubleByteCmd = ch;		// 09 is followed by some 0 chars reason unknown but we can ignore it
			// 0B 01  flashing text on  0B 00 flashing text off
			if (ch == 0x0B) iDoubleByteCmd = ch;
			// 0D 03  save next text in buffer for repeated use 04 04
			if (ch == 0x0D) iDoubleByteCmd = ch;
			// CPU ask for identification use only when we are the soul controller (so we do also all the key-presses)
			// if (ch == 0x0F) { con.writef("200x/700x A02  "); con.write("\x80\x00",2); }
			if (ch == 0xF) continue;					// ignore otherwise
			if (iDoubleByteCmd) continue;
			// 06 07 (tipple byte command)
			//if (ch == 0x0F){ iDoubleByteCmd = 4; ch = 0;}
		}


		if (iDoubleByteCmd)
		{
			if (iDoubleByteCmd == 0x0B) { flashChars = (ch != 0); iDoubleByteCmd = 0; continue; }		// start stop flashing of the ASCII chars
			iDoubleByteCmd |= (ch << 8);
			handleCompleteMessage();
			iCurMsgType = iDoubleByteCmd;
			iDoubleByteCmd = 0;
			// if we start at a offset get last complete message from memory
			uint8_t iCmd    = iCurMsgType & 0xFF;
			uint8_t iCmdPar = iCurMsgType >> 8;
			// in some cases 0xD is used a <CR> and we overwrite the current line starting at position 0
			if ((iCmd == 0x0D) && (iCmdPar >= 0x10))
			{
				iCurMsgType = 0x04;
				msg.set(msgLatest);
				if (msg.length() == 0)
					msg.append((char)iCmdPar);
				else
					msg[0] = iCmdPar;
				iMsgDupOffset = 1;
			}
			if ((iCmd == 4) && (iCmdPar > 0))
			{
				if (msgDup.length() > 0) msg.set(msgDup);
				else msg.set(msgLatest);
				iMsgDupOffset = iCmdPar;
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
			continue;
		}
		if ((iCurMsgType == 0) && (ch >= 0x20) && (ch <= 0xB0))
			iCurMsgType = 0x04;

		if (iCurMsgType) 
		{
			if ((flashChars) && ((iCurMsgType & 0xFF) == 04)) ch |= 0x80;

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

ansStl::cST keithleyDisplay::getDispHeadTxts()
{
	ansStl::cST res;
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

#define _PrintCmds_

#ifdef _PrintCmds_
void showDisplay(ansStl::cST msg, ansStl::cST& msDup, ansStl::cST& msLst)
{
    #if defined(PICO_RP2040)
	for (int i = 0; i < msg.length(); i++) if ((((uint8_t)msg[i]) >= 0x7F) || (msg[i] < 32)) msg[i] = '.';
	#else
	for (int i = 0; i < msg.length(); i++) if (((uint8_t)msg[i]) > 0xB0) msg[i] &= 0x7F;
	#endif
	printf("-%2d-%2d-%2d-\"%s\"\r\n", msDup.length(), msLst.length(), msg.length(), msg.buf());
}
#endif


void keithleyDisplay::handleCompleteMessage()
{
	if (iCurMsgType == 0) return;
	uint8_t iCmd = iCurMsgType & 0xFF;
	switch (iCmd)
	{
	case 0x0D:		
		// 0D 02 and 0D 03 seen as memory dup line 
		// 0D followed by char >= 20h (space) looks like normal EOL type of commit
		msgDup.set(msg);
	case 0x04:
	{
#ifdef _PrintCmds_
		uint8_t iCmdPar = iCurMsgType >> 8;
		printf("-- %02X %02X --", iCmd, iCmdPar);
		showDisplay(msg, msgDup, msgLatest);
#endif
		bValidCom = true;
		//printf("string %s", msg.buf());
		msgLatest = msg;
		bIsUpdated = true;
		break;
	}
	case 6:
		/*  Filter off
		** enable rear flt auto arm  0x4019  0x4000  rear
		** 06 40 19  == 0x4019  (rear,arm,auto,flt)                        
        ** 07 BF E6  xor 0xFFFF  of the 06 frame    
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
				bIsUpdated = true;
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

void keithleyDisplay::saveBuffer()
{
	ansStl::cST sFN;
	ansStl::cST hMSG;
	int iLeng = (_RingSize_ - 100) & 0xFFFE0;
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
				hMSG.append(" %02X", ringBuffer[idx] & 0xFF);
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
				hMSG.append("%3d", diffMS);
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
		printf("saved to %s done", sFN.buf());
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

