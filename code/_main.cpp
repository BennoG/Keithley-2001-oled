#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "keithleyDisplay.h"
#include "pico/bootrom.h"
#include "SSD1322.h"

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
            if (sConMsg.length() > 0) sConMsg.insDel(-1,-1);
            printf("Command='%s'\n", sConMsg.buf());
            continue;
        }
        if ((c == '\n') || (c == '\r')) {
            if (sConMsg.length() > 0 )
            {
                printf("Received message: %s\n", sConMsg.buf());
                if (sConMsg.comparei("boot")) reset_usb_boot(0, 0);
                sConMsgLast.set(sConMsg);
                sConMsg.clear();
                bConMsgDone = true;
            }
            continue;
        } else sConMsg.append((char)c);
        if (sConMsg.length() > 0)
            printf("Command='%s'\n", sConMsg.buf());
        else
            printf("Received char: 0x%02X %c\n", c, c > 31 ? c : '.');
        if (c == 'B') reset_usb_boot(0, 0);
    }
}


/* handle all commands or characters recieved from the console
** so the user can interact with the program via the console, for example to trigger a reset to boot mode or to send commands to the Keithley display
*/
void handleConsoleData(consoleDataHandler& KH)
{
    KH.poll();
    if (KH.hasSpecialMsg()) {
        uint32_t msg = KH.getSpecialMsg();
        printf("Received special message: 0x%08X\n", msg);
        if (msg == 0x005B3131) reset_usb_boot(0, 0);   // F1 key sequence (go to boot mode) 
    }
    if (KH.hasCommand()) {
        ansStl::cST cmd;
        KH.getCommand(cmd);
        printf("Received command: %s\n", cmd.buf());
        if (cmd.comparei("boot")) reset_usb_boot(0, 0);
    }
}


int main()
{
    initHardware();
 
    consoleDataHandler KH;
    keithleyDisplay KD(UART_ID);
	SSD1322 oled;
	oled.init();

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
        handleConsoleData(KH);
        
		if (KD.hasUpdate())
		{
			msgHD = KD.getDispHeadTxts();
			msgFT = KD.getUpdate();				// both lines (values and 2nd row)
			msgValue = msgFT.substr(0, 20);		// big digits
			msgFT.insDel(0, -20);				// bottom line is remainder
			doRedraw = true;
		}

        // handle flashing of characters if needed and redraw the display
        uint32_t difMS = stlMsTimer(lastFlipMS);
		if ((msgValue.length() > 0) && (difMS > 200u))
		{
			if (difMS < 1000u) difMS = 200;		// so we have a stable flash time
			lastFlipMS += difMS;
			oled.colorFlash = oled.colorFlash == 2 ? 15 : 2;
			doRedraw = true;
		}

        // redraw the display if we have received new data or if we need to update the flashing of characters
		if (doRedraw)
		{
			doRedraw = false;
            gpio_put(20, 1);
			oled.clear();
			int x = 128 - msgHD.length() * 3;
			oled.drawString(x, 0, msgHD.buf(), 8, 0);			// top line (ARM AUTO..) text
			oled.drawStringB(4, 16, msgValue.buf(), 10, 0);	    // values text
			oled.drawString(2, 55, msgFT.buf(), 8, 0, 2);		// bottom line somewhat wider chars
			oled.update();
            gpio_put(20, 0);
        }
        gpio_put(21, loopTel & 1);      // toggle pin to measure loop time with an oscilloscope
    }
}
