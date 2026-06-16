#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "keithleyDisplay.h"
#include "pico/bootrom.h"
#include "SSD1322.h"
#define PROGMEM
#include "font/calibri5pt7b.h"
#include "font/calibri6pt7b.h"
#include "font/calibri7pt7b.h"
#include "font/calibri8pt7b.h"
//#include "font/NotoMono15pt.h"
#include "font/FreeMono18pt7b.h"
#include "font/FreeMono24pt7b.h"
#include "font/NotoMono_Regular24pt7b.h"
#include "font/NotoMono_Regular22pt7b.h"
#include "font/NotoMono_Regular20pt7b.h"
#include "font/NotoMono_Regular19pt7b.h"
#include "font/NotoMono_Regular18pt7b.h"
#include "font/NotoMono_Regular17pt7b.h"
#include "font/NotoMono_Regular16pt7b.h"
#include "font/NotoMono_Regular15pt7b.h"
#include "font/NotoMono_Regular5pt7b.h"
#include "font/NotoMono_Regular6pt7b.h"
#include "font/NotoMono_Regular7pt7b.h"
#include "font/NotoMono_Regular8pt7b.h"
#include "font/keithleySpecial5x7.h"

int iDebugBits = 0;			// for debugging so we can log different kind of events

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 9600

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

static void initHardware() 
{
    stdio_init_all();

    // some pins for debugging with an oscilloscope
    gpio_init(20);
    gpio_init(21);
    gpio_set_dir(20, GPIO_OUT);
    gpio_set_dir(21, GPIO_OUT);

    // Set up our UART
    //uart_set_format(UART_ID,8,1,UART_PARITY_NONE);
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
}

class consoleDataHandler
{
    public:
        consoleDataHandler() { initVars(); };
        ~consoleDataHandler() {};
        bool hasSpecialMsg() { return bEscMsgDone; };
        uint32_t getSpecialMsg() { bEscMsgDone = false; return iEscMsg; };
        bool hasCommand(){ return bConMsgDone; };
        int  getCommand(ansStl::cST& msg);
        void poll();
        uint32_t getLastKey() {  uint32_t key = lastReceivedKey; lastReceivedKey = 0; return key; };
    private:
    void initVars() {
        bEscMsgDone = false;
        bConMsgDone = false;
        lastReceivedMS = 0;
        iEscMsg = 0;
        iEscMsgLen = 0;
    }
    ansStl::cST sConMsg;        // for receiving console messages
    ansStl::cST sConMsgLast;    // last received console message, 
    bool bConMsgDone;           // true if a complete console message has been received and is ready for processing
    uint32_t iEscMsg;           // for the special character sequence
    int iEscMsgLen;             // total numer of characters received in the special character sequence
    bool bEscMsgDone;           // true if the special character sequence is complete and ready for processing
    uint32_t lastReceivedMS;	// ms timer last character from the serial port for the special character sequence
    uint32_t lastReceivedKey;   // last keypress we have received
};

int consoleDataHandler::getCommand(ansStl::cST& msg)
{
    if (!bConMsgDone) return 0;
    msg.set(sConMsgLast);
    bConMsgDone = false;
    return msg.length();
}

void consoleDataHandler::poll()
{
    // timeout for the special character sequence is 5 ms, if we have no data for 5ms then we can assume the message is complete
    if (lastReceivedMS != 0)
    {
        uint32_t diffMS = stlMsTimer(lastReceivedMS);
        if ((diffMS > 5) || (iEscMsgLen >= 4)) { bEscMsgDone = true; lastReceivedMS = 0; }
    }
 
    while (true) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) break;
        if (c == 0) continue;   // ignore null characters

        if (lastReceivedMS != 0) {
            if (c == 0x7E) { bEscMsgDone = true; lastReceivedMS = 0; continue; }   // end of special character sequence
            iEscMsg = (iEscMsg << 8) | (c & 0xFF);
            iEscMsgLen++;   
            continue;
        }

        if (c == 0x1B) {   // start of special character sequence
            iEscMsg = 0;
            iEscMsgLen = 1;
            bEscMsgDone = false;
            lastReceivedMS = stlMsTimer(0);
            continue;
        }
        if (c == 0x7F) {   // backspace character, remove last character from the console message
            if (sConMsg.length() > 0){ sConMsg.insDel(-1,-1); putchar(c); }
            //printf("Command='%s'\n", sConMsg.buf());
            continue;
        }
        if ((c == '\n') || (c == '\r')) {
            if (sConMsg.length() > 0 )
            {
                printf("\r\nReceived message: %s\n", sConMsg.buf());
                if (sConMsg.comparei("boot")) reset_usb_boot(0, 0);
                sConMsgLast.set(sConMsg);
                sConMsg.clear();
                bConMsgDone = true;
            }
            continue;
        } else{
            sConMsg.append((char)c);
            //printf("\r%s",sConMsg.buf());
            putchar(c);
        } 
    }
}



static void printHelp()
{
    printf("Possible commands\r\n");
    printf("  boot    reboot in boot mode\r\n");
    printf("  dump    dump last 4000 received chars\r\n");
    printf("  debug [val] debug bits get or set\r\n");
    printf("  debughelp   show all possible debug flags\r\n");
    printf("\r\n");
}

static void handleDebugHelp()
{
    printf("debug 1h   show received display messages\r\n");
    printf("debug 2h   also show stored lengths\r\n");
    printf("debug 4h   do anti alias on the whole screen\r\n");
    printf("debug 8h   drop GPIO20 before sending SPI\r\n");
}

static void handleDebug(ansStl::cST &cmdLin)
{
    ansStl::cST val = cmdLin.getDlm(2,' '); // substr(5, 10);
    if (val.comparei("help")) { handleDebugHelp(); return; }
    if (val.length() > 0)
    {
        iDebugBits =  val.toInt(); //  trtol(val.buf(),NULL,0); //atoi(val.buf());
    }
    printf("debug %d 0x%X\r\n", iDebugBits, iDebugBits);
}

/* handle all commands or characters recieved from the console
** so the user can interact with the program via the console, for example to trigger a reset to boot mode or to send commands to the Keithley display
*/
void handleConsoleData(consoleDataHandler& KH,keithleyDisplay& KD)
{
    KH.poll();
    if (KH.hasSpecialMsg()) {
        uint32_t msg = KH.getSpecialMsg();
        printf("Received special message: 0x%08X\n", msg);
        if (msg == 0x005B3131) reset_usb_boot(0, 0);   // F1 key sequence (go to boot mode) 
    }
    if (KH.hasCommand()) {
        ansStl::cST cmdLin,cmd;
        KH.getCommand(cmdLin);
        cmd = cmdLin.getDlm(1, ' ');
        printf("Received command: %s\r\n", cmdLin.buf());
        if (cmd.comparei("boot")) reset_usb_boot(0, 0);
        else if (cmd.comparei("dump")) KD.saveBuffer(cmdLin.getDlm(2, ' ').toInt());
        else if (cmd.comparei("help")) printHelp();
        else if (cmd.startsWith("debughelp",false)) handleDebugHelp();
        else if (cmd.startsWith("debug",false)) handleDebug(cmdLin);
        else printf("Unknown command type help for commands\r\n");
    }
}


int main()
{
    initHardware();
 
    consoleDataHandler KH;
    keithleyDisplay KD(UART_ID);
	SSD1322 oled;
	oled.init();
    oled.clear();
    KD.iDeviceType = 2001;
    oled.curFontSspec = &keithleySpecial5x7;
    //oled.curFontB = &NotoMono_Regular18pt7b;    // 25 pixels high
    //oled.curFontB = &NotoMono_Regular24pt7b;    // 25 pixels high
    //oled.curFontBhigh = 25;

    uint8_t loopTel = 0;

	ansStl::cST msgHD;		// head row text
	ansStl::cST msgFT;		// footer row text
	ansStl::cST msgValue;	// value text
	bool doRedraw = false;
	uint32_t lastFlipMS = stlMsTimer(0);

    while (true) {
        loopTel++;
   
        // process any incoming data from the Keithley display
        KD.poll();
        // process any incoming data from the console (USB uart)
        handleConsoleData(KH, KD);
             
		if (KD.hasUpdate())
		{
			msgHD = KD.getDispHeadTxts();
			msgFT = KD.getUpdate();				    // both lines (values and 2nd row)
            // the K2000 has 15 chars on the main row
            // the K2001 (7001) has 20 on the first row and 32 on the 2nd, both lines are transmitted in 1 single message
            if (msgFT.length() > 0)
            {
                msgValue = msgFT.substr(0, 20);		// big digits
                msgFT.insDel(0, -20);				// bottom line is remainder
                doRedraw = true;
            }
		}

        // handle flashing of characters if needed and redraw the display
        uint32_t difMS = stlMsTimer(lastFlipMS);
		if ((msgValue.length() > 0) && (difMS > 200u) && (KD.isFlashing()))
		{
			if (difMS < 1000u) difMS = 200;		// so we have a stable flash time
			lastFlipMS += difMS;
			oled.colorFlash = oled.colorFlash == 2 ? 15 : 2;
			doRedraw = true;
		}

        // redraw the display if we have received new data or if we need to update the flashing of characters
        // in the fastest mode we have about 25 ms per display upat to do the update
        // 15.3 ms update time for the 18pt font with AA
        // 11.5 ms update time for the 18pt font without AA
		if (doRedraw)
		{
			doRedraw = false;
            gpio_put(20, 1);                                    // GPIO 20 high for scope timing measurement
			oled.clear();
            if (KD.iDeviceType == 2001)
            {
                oled.curFontB = &NotoMono_Regular16pt7b;            // 25 pixels high
                oled.curFontBhigh = 25;
                int x = 128 - msgHD.length() * 3;                   // calculate the starting position of head text so it is in the horizontal center
                oled.drawString(x, 0, msgHD.buf(), 8, 0);			// top line (ARM AUTO..) text
                oled.drawStringB(4, 32-(oled.curFontBhigh/2), msgValue.buf(), 10, 0, -2);	// values text
                if (iDebugBits & 4)
                    oled.fastAA(19,49);                             // 4.5ms do anti aliassing on the whole screen (can be reduced if we onlo do the lines of the big chars)
                oled.hLine(0,22,50,8);
                oled.hLine(0,47,50,8);
                oled.drawString(2, 55, msgFT.buf(), 8, 0, 2);		// bottom line somewhat wider chars
                if (iDebugBits & 8) gpio_put(20, 0);                // debug 0x008 lower GPIO20 before sending the SPI screen data
            }else{
                oled.curFontB = &NotoMono_Regular20pt7b;            // 30 pixels high
                oled.curFontBhigh = 30;
                oled.drawStringB(4, 32-(oled.curFontBhigh/2), msgValue.buf(), 10, 0, -3);	// values text
                if (iDebugBits & 4)
                    oled.fastAA(19,49);                             // 4.5ms full and 3.7ms partial anti aliassing
                //oled.hLine(0,19,50,8);
                //oled.hLine(0,49,50,8);

                int x = 128 - msgHD.length() * 3;                   // calculate the starting position of head text so it is in the horizontal center
                oled.drawString(x, 0, msgHD.buf(), 8, 0);			// top line (ARM AUTO..) text
                //msgFT.clear(); msgFT.append("\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F");
                //oled.drawString(2, 55, msgFT.buf(), 8, 0, 0);   // bottom line somewhat wider chars
                //msgFT.set(" !\"#$%&'()*+,-./012345");
                //oled.drawString(&keithleySpecial5x7,128, 55, msgFT.buf(), 8, 0, 0);   // bottom line somewhat wider chars
                //oled.drawString(&keithleySpecial5x7,94, 55, "-./0", 8, 0, 0);   // bottom line somewhat wider chars
                if (iDebugBits & 8) gpio_put(20, 0);                // debug 0x008 lower GPIO20 before sending the SPI screen data
            }

			oled.update();                                          // 9.0 ms redraw screen from memeory this takes about 9 ms
            gpio_put(20, 0);
        }
        gpio_put(21, loopTel & 1);      // toggle pin to measure loop time with an oscilloscope
    }
}
