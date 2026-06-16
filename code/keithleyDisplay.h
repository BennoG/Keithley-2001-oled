#ifdef _WIN32
#  include "stl_str.h"
#  include "stl_conio.h"
#  include "stl_rs232.h"
#else
#  include <vector>
#  include <string.h>
#  include "pico/stdlib.h"
#  include "hardware/timer.h"
#  include "hardware/uart.h"
#  include "cST.h"
static uint32_t stlMsTimer(uint32_t uOld)
{
    uint64_t uNew = time_us_64()/1000;
    return (uint32_t)(uNew  - uOld);
}

namespace ansStl
{
    class rs232
    {
        private:
        /* data */
        uart_inst_t *m_Puart;
        public:
        rs232() { m_Puart = NULL;};
        ~rs232() {};
        void connect(uart_inst_t *pUart){ m_Puart = pUart; }
        int getc(int iTimoutMS)
        {
            
            if (m_Puart == NULL) return -1;
            if (uart_is_readable_within_us(m_Puart, iTimoutMS * 1000)) 
            return uart_getc(m_Puart) ;
            return -1;
        }
    };
}

#endif

class keithleyDisplay
{
#define _RingSize_	 5000
public:
	keithleyDisplay();
	keithleyDisplay(std::vector<uint32_t>& simData);
	#ifdef _WIN32
    keithleyDisplay(const char* sPortName);
    #endif
	#if defined(PICO_RP2040)
	keithleyDisplay(uart_inst_t *pUart);
    #endif
	~keithleyDisplay();
	void poll();
    void saveBuffer(int iDumpLen = 0);
	bool hasUpdate() { return bIsUpdated; }
	ansStl::cST getDispHeadTxts();
	ansStl::cST getUpdate() { bIsUpdated = false; return msgLatest; };
	bool isFlashing(){ return bLastMsgFlash; }
	int iDeviceType;		// currently only 2000 and 2001
private:
	void initVars() {
		iCurMsgType = 0;  bValidCom = false; iReadLeng = 0; bUnExpected = false; bIsUpdated = false; flashChars = false;
		bCurMsgFlash = false; bLastMsgFlash = false;
		iInsPosition = -1; iDoubleByteCmd = 0; flashChars = false; iMsgDupOffset = 0; msg67bits = 0;    
		bSingleByteOptions = true; iDeviceType = 0; 
		k2000bits.clear(); k2000bits.insDel(0, 16);	k2000bitsOld.clear(); k2000bitsOld.insDel(0, 16);
         #ifdef _RingSize_
		lastReceivedMS = 0;
        memset(ringBufferMS, 0, sizeof(ringBufferMS));
        memset(ringBuffer, 0, sizeof(ringBuffer)); 
        ringHead = 0;
        #endif
	}
	ansStl::rs232 con;
	ansStl::cST k2000bits;		// for now we do 16 bytes problably only need offset 06 07 08 09 0A and 0E
	ansStl::cST k2000bitsOld;	// for now we do 16 bytes problably only need offset 06 07 08 09 0A and 0E
	int msg67bits;
	ansStl::cST msg, msg7, msg6;
	ansStl::cST msgLatest;
	bool bCurMsgFlash,bLastMsgFlash;
	bool bIsUpdated;			// true if we have a new message for the display
	// options around the display are send using 1 or 2 byte data commands depending on the type of display (matrix or 14 segment)
	// the K2000 uses 14 segment digits
	// the K2001 K2002 K2400 K7001 uses matrix digits
	// the 14 segment displays use 1 data byte for the 06 07 08 09 0A and 0E messages
	// the matrix digit displays uses 2 data bytes for the 06h and 07h messages for the disply options
	bool bSingleByteOptions;	// for the K2000 we have single byte options for the 06 and 07 commands


	int iMsgDupOffset;			// offset in previous message for repeated use with minor differences
	int iCurMsgType;			// message type we are receiving at this moment
	int iReadLeng;				// used if we need to read a fixed length of characters
	int  iInsPosition;			// for overwrite of the previous string
	int  iDoubleByteCmd;		// if command is double byte the this is the first byte of the command
	bool bUnExpected;       	// we have send the unexpeded char message to the debug output so we don't need to repeat it for every char until the next command
    bool bValidCom;				// we have valid communication seen
	bool flashChars;			// true if newly received chars need to be flashing shown.
	uint32_t lastReceivedMS;	// ms timer last character from the serial port
	void handleCompleteMessage();
 
#ifdef _RingSize_
    char ringBuffer[_RingSize_];
	uint32_t ringBufferMS[_RingSize_];
	int  ringHead;
	void printHex(int iSource);
#endif
};

