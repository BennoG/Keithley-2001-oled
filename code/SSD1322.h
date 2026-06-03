#ifndef _WIN32

#include <vector>
#include <stdio.h>

#if defined(PICO_RP2040)
#  include "pico/stdlib.h"
#  include "hardware/spi.h"
#else
#  include "stl_str.h"
#  include <gpiod.h>
#endif

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
/*
#define SPI_PORT spi0
#define PIN_MISO 4
#define PIN_CS   1
#define PIN_SCK  6
#define PIN_MOSI 7
*/
#define SPI_PORT spi1
#define PIN_SCK     10
#define PIN_MOSI    11
#define PIN_MISO    12
#define PIN_CS      13
#define PIN_DCA     14
#define PIN_RESET   15


/*  wiring
| SSD1322    | Raspberry Pi |
| -------    | ------------ |
|  1 VCC     |    3.3V      |
|  2 GND     |  6 GND       |
|  4 CLK     | 23 SCLK      |
|  5 DIN     | 19 MOSI      |
| 16 CS      | 24 CE0       |
| 14 DC      | 22 GPIO25    |
| 15 RES     | 18 GPIO24    |
*/

class SSD1322 {
public:

	SSD1322();
	~SSD1322();

	void init();
	void close();
	void clear(uint8_t gray = 0);
	void setPixel(int x, int y, uint8_t gray);
	uint8_t getPixel(int x, int y) const;
	void fillRect(int x, int y, int w, int h, uint8_t gray);
	void hLine(int x, int y, int len, uint8_t gray);
	void vLine(int x, int y, int len, uint8_t gray);
	void drawRect(int x, int y, int w, int h, uint8_t gray);
	void drawCircle(int cx, int cy, int r, uint8_t gray);
	void drawChar(int x, int y, uint8_t c, uint8_t fg, uint8_t bg = 0);
	void drawCharB(int x, int y, uint8_t c, uint8_t fg, uint8_t bg = 0);
	void drawString(int x, int y, const char* str, uint8_t fg, uint8_t bg = 0,int iHorExtra = 0);
	void drawStringB(int x, int y, const char* str, uint8_t fg, uint8_t bg = 0);
	void update();
	void setContrast(uint8_t val);
	void sleep(bool on);
	uint8_t colorFlash;
private:
	void initVars() 
	{
	#if defined(PICO_RP2040)
		spi_ = SPI_PORT; line_dc_ = PIN_DCA; line_rst_ = PIN_RESET;
	#else  // pico
		spi_fd_ = -1; chip_ = nullptr; line_dc_ = nullptr; line_rst_ = nullptr;
	#endif //  pico 
		colorFlash = 4;
		DISPLAY_WIDTH = 256; DISPLAY_HEIGHT = 64;
		fb_.resize((DISPLAY_HEIGHT * DISPLAY_WIDTH) / 2, 0);
	}
	// config
	int DISPLAY_WIDTH;
	int DISPLAY_HEIGHT;
	
	// -- SPI / GPIO handles --------------------------------------------------
	#if defined(PICO_RP2040)
	spi_inst_t* spi_;
	int line_dc_;
	int line_rst_;
	#else  // pico
	int         spi_fd_;
	gpiod_chip* chip_;
	gpiod_line* line_dc_;
	gpiod_line* line_rst_;
	#endif //  pico

	// -- Framebuffer: 128 bytes * 64 rows = 8192 bytes -----------------------
	// -- 2 pixels per byte
	std::vector<uint8_t> fb_;

	void openSPI();

	void openGPIO();
	void setDC(int v);
	void setRST(int v);
	void reset();
	void spiWrite(const uint8_t* buf, size_t len);
	template<typename... Args> void sendCmd(uint8_t cmd, Args... params);
	void sendData(const uint8_t* buf, size_t len);
	void initSequence();
};


#endif	// _WIN32

