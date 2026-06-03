#ifdef _WIN32
#  include "stl_str.h"
#  include "stl_conio.h"
#  include "stl_rs232.h"
#else
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
#endif

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
	        #if defined(PICO_RP2040)
            
            if (m_Puart == NULL) return -1;
            if (uart_is_readable_within_us(m_Puart, iTimoutMS * 1000)) 
                return uart_getc(m_Puart) ;
            return -1;
            #else
            while (true)
            {
               if (uart_is_readable(m_Puart))
                    return uart_getc(m_Puart);
                if (iTimoutMS <= 0) return -1;
                iTimoutMS--;
                sleep_ms(1);
            }
              return -1;
            #endif
        }
    };
}

class keithleyDisplay
{
#define _RingSize_	 5000
public:
	keithleyDisplay();
	keithleyDisplay(uart_inst_t *pUart);
	~keithleyDisplay();
	void poll();
	void saveBuffer();
	bool hasUpdate() { return bIsUpdated; }
	ansStl::cST getDispHeadTxts();
	ansStl::cST getUpdate() { bIsUpdated = false; return msgLatest; };
private:
	void initVars() {
		iCurMsgType = 0;  bValidCom = false; iReadLeng = 0; bUnExpected = false;
		iInsPosition = -1; iDoubleByteCmd = 0; flashChars = false; iMsgDupOffset = 0; msg67bits = 0;    
         #ifdef _RingSize_
		lastReceivedMS = 0;
        memset(ringBufferMS, 0, sizeof(ringBufferMS));
        memset(ringBuffer, 0, sizeof(ringBuffer)); 
        ringHead = 0;
        #endif
	}
	ansStl::rs232 con;
	
	int msg67bits;
	ansStl::cST msg, msg7, msg6;
	ansStl::cST msgDup;
	
	ansStl::cST msgLatest;
	bool bIsUpdated;

	int iMsgDupOffset;		// offset in previous message for repeated use with minor differences
	int iCurMsgType;
	int iReadLeng;			// used if we need to read a fixed length of characters
	int  iInsPosition;		// for overwrite of the previous string
	int  iDoubleByteCmd;	// if command is double byte the this is the first byte of the command
	bool bUnExpected;       // we have send the unexpeded char message to the debug output so we don't need to repeat it for every char until the next command
    bool bValidCom;
	bool flashChars;		// true if newly received chars need to be flashing shown.
	uint32_t lastReceivedMS;	// ms timer last character from the serial port
	void handleCompleteMessage();
    #ifdef _RingSize_
	char ringBuffer[_RingSize_];
	uint32_t ringBufferMS[_RingSize_];
	int  ringHead;
	void printHex(int iSource);
    #endif
};

