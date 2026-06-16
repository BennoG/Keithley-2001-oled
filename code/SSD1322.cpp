#ifndef _WIN32

#if defined(PICO_RP2040)

#else
#  include <linux/spi/spidev.h>
#  include <sys/ioctl.h>
#endif

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <thread>

extern int iDebugBits;			// for debugging so we can log different kind of events (defined in main.cpp)

#include "SSD1322.h"

// --- SSD1322 Commands ---------------------------------------------------------
enum SSD1322Cmd : uint8_t {
	CMD_SET_COL_ADDR = 0x15,
	CMD_WRITE_RAM = 0x5C,
	CMD_READ_RAM = 0x5D,
	CMD_SET_ROW_ADDR = 0x75,
	CMD_SET_REMAP = 0xA0,
	CMD_SET_DISPLAY_START = 0xA1,
	CMD_SET_DISPLAY_OFFSET = 0xA2,
	CMD_DISPLAY_NORMAL = 0xA4,   // show GDDRAM content
	CMD_DISPLAY_ALL_ON = 0xA5,
	CMD_DISPLAY_ALL_OFF = 0xA6,
	CMD_DISPLAY_INVERT = 0xA7,
	CMD_SET_MUX_RATIO = 0xCA,
	CMD_SET_GPIO = 0xB5,
	CMD_SET_FUNC_SEL = 0xAB,
	CMD_SET_PHASE_LEN = 0xB1,
	CMD_SET_CLK_DIV = 0xB3,
	CMD_SET_VSL = 0xB4,
	CMD_SET_GRAY_SCALE = 0xB8,
	CMD_DEFAULT_GRAY_SCALE = 0xB9,
	CMD_SET_PRECHARGE_V = 0xBB,
	CMD_SET_VCOMH = 0xBE,
	CMD_SET_CONTRAST = 0xC1,
	CMD_MASTER_CONTRAST = 0xC7,
	CMD_SET_PRECHARGE_2 = 0xD1,  // actually 0xB6 on some docs; use 0xB6
	CMD_SET_PRECHARGE2 = 0xB6,
	CMD_SLEEP_ON = 0xAE,	// display off
	CMD_SLEEP_OFF = 0xAF,	// display on
	CMD_ENHANCE_A = 0xB4,  // overlaps VSL; see usage in init
};

// example for exclamation mark '!' (0x21):
// 01  X
// 02  X
// 04  X
// 08  X
// 10  X
// 20
// 40  X
// 80

const uint8_t k2001Remap[][2]
{
	// original - replacement special font
	{ 0x10,0x20 },	// 0x10		B5 u (micro)
	{ 0x11,0x21 },	// 0x11
	{ 0x12,0x22 },	// 0x12     AF Ohm (Omega)
	{ 0x13,0x23 },	// 0x13     B0 o (Celsius degree)
	{ 0x14,0x24 },	// 0x14     bar graph stand left block
	{ 0x15,0x25 },	// 0x15     bar graph stand right block
	{ 0x16,0x26 },	// 0x16     bar graph mid bard
	{ 0x17,0x27 },	// 0x17     bar graph left block
	{ 0x18,0x28 },	// 0x18     bar graph right block
	{ 0x19,0x29 },	// 0x19     bar graph  block
	{ 0x1A,0x2A },	// 0x1A     ^ up arrow
	{ 0x1B,0x2B },	// 0x1B     v down arrow
	{ 0x1C,0x2C },	// 0x1C		< left arrow
	{ 0x1D,0x2D },	// 0x1D     > rithg arrow
	{ 0x1E,0x2E },	// 0x1E		display test pattern
	{ 0x1F,0x2F },	// 0x1F     display test pattern

	{ 0x90,0x20 },	// 0x90  horizontal top line
	{ 0x91,0x21 },	// 0x91  horizontal 2nd line
	{ 0x92,0x22 },	// 0x92  horizontal 3th line
	{ 0x93,0x23 },	// 0x93  horizontal 4th line
	{ 0x94,0x24 },	// 0x94  horizontal 5th line
	{ 0x95,0x25 },	// 0x95  horizontal 6th line
	{ 0x96,0x26 },	// 0x96  horizontal bottom line
	{ 0x97,0x27 },	// 0x97  vertical left
	{ 0x98,0x28 },	// 0x98  vertical 2nd
	{ 0x99,0x29 },	// 0x99  vertical 3th
	{ 0x9A,0x2A },	// 0x9A  vertical 4th
	{ 0x9B,0x2B },	// 0x9B  vertical right
	{ 0x9C,0x2C },	// 0x9C  Up and Down arrow
};



const uint8_t font5x7L[][5] = {	// k7001 display special characters 0x00 - 0x9F Below 0x20 and above 0x7F are mostly blank, but some are used for special symbols and bar graph elements
	{0x00,0x00,0x00,0x00,0x00}, // ' '
	{0x00,0x00,0x5F,0x00,0x00}, // '!'
	{0x00,0x07,0x00,0x07,0x00}, // '"'
	{0x14,0x7F,0x14,0x7F,0x14}, // '#'
	{0x24,0x2A,0x7F,0x2A,0x12}, // '$'
	{0x23,0x13,0x08,0x64,0x62}, // '%'
	{0x36,0x49,0x55,0x22,0x50}, // '&'
	{0x00,0x05,0x03,0x00,0x00}, // '\''
	{0x00,0x1C,0x22,0x41,0x00}, // '('
	{0x00,0x41,0x22,0x1C,0x00}, // ')'
	{0x14,0x08,0x3E,0x08,0x14}, // '*'
	{0x08,0x08,0x3E,0x08,0x08}, // '+'
	{0x00,0x50,0x30,0x00,0x00}, // ','
	{0x08,0x08,0x08,0x08,0x08}, // '-'
	{0x00,0x60,0x60,0x00,0x00}, // '.'
	{0x20,0x10,0x08,0x04,0x02}, // '/'
	{0x3E,0x51,0x49,0x45,0x3E}, // '0'
	{0x00,0x42,0x7F,0x40,0x00}, // '1'
	{0x42,0x61,0x51,0x49,0x46}, // '2'
	{0x21,0x41,0x45,0x4B,0x31}, // '3'
	{0x18,0x14,0x12,0x7F,0x10}, // '4'
	{0x27,0x45,0x45,0x45,0x39}, // '5'
	{0x3C,0x4A,0x49,0x49,0x30}, // '6'
	{0x01,0x71,0x09,0x05,0x03}, // '7'
	{0x36,0x49,0x49,0x49,0x36}, // '8'
	{0x06,0x49,0x49,0x29,0x1E}, // '9'
	{0x00,0x36,0x36,0x00,0x00}, // ':'
	{0x00,0x56,0x36,0x00,0x00}, // ';'
	{0x08,0x14,0x22,0x41,0x00}, // '<'
	{0x14,0x14,0x14,0x14,0x14}, // '='
	{0x00,0x41,0x22,0x14,0x08}, // '>'
	{0x02,0x01,0x51,0x09,0x06}, // '?'
	{0x32,0x49,0x79,0x41,0x3E}, // '@'
	{0x7E,0x11,0x11,0x11,0x7E}, // 'A'
	{0x7F,0x49,0x49,0x49,0x36}, // 'B'
	{0x3E,0x41,0x41,0x41,0x22}, // 'C'
	{0x7F,0x41,0x41,0x22,0x1C}, // 'D'
	{0x7F,0x49,0x49,0x49,0x41}, // 'E'
	{0x7F,0x09,0x09,0x09,0x01}, // 'F'
	{0x3E,0x41,0x49,0x49,0x7A}, // 'G'
	{0x7F,0x08,0x08,0x08,0x7F}, // 'H'
	{0x00,0x41,0x7F,0x41,0x00}, // 'I'
	{0x20,0x40,0x41,0x3F,0x01}, // 'J'
	{0x7F,0x08,0x14,0x22,0x41}, // 'K'
	{0x7F,0x40,0x40,0x40,0x40}, // 'L'
	{0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
	{0x7F,0x04,0x08,0x10,0x7F}, // 'N'
	{0x3E,0x41,0x41,0x41,0x3E}, // 'O'
	{0x7F,0x09,0x09,0x09,0x06}, // 'P'
	{0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
	{0x7F,0x09,0x19,0x29,0x46}, // 'R'
	{0x46,0x49,0x49,0x49,0x31}, // 'S'
	{0x01,0x01,0x7F,0x01,0x01}, // 'T'
	{0x3F,0x40,0x40,0x40,0x3F}, // 'U'
	{0x1F,0x20,0x40,0x20,0x1F}, // 'V'
	{0x3F,0x40,0x38,0x40,0x3F}, // 'W'
	{0x63,0x14,0x08,0x14,0x63}, // 'X'
	{0x07,0x08,0x70,0x08,0x07}, // 'Y'
	{0x61,0x51,0x49,0x45,0x43}, // 'Z'
	{0x00,0x7F,0x41,0x41,0x00}, // '['
	{0x02,0x04,0x08,0x10,0x20}, // '\'
	{0x00,0x41,0x41,0x7F,0x00}, // ']'
	{0x04,0x02,0x01,0x02,0x04}, // '^'
	{0x40,0x40,0x40,0x40,0x40}, // '_'
	{0x00,0x01,0x02,0x04,0x00}, // '`'
	{0x20,0x54,0x54,0x54,0x78}, // 'a'
	{0x7F,0x48,0x44,0x44,0x38}, // 'b'
	{0x38,0x44,0x44,0x44,0x20}, // 'c'
	{0x38,0x44,0x44,0x48,0x7F}, // 'd'
	{0x38,0x54,0x54,0x54,0x18}, // 'e'
	{0x08,0x7E,0x09,0x01,0x02}, // 'f'
	{0x0C,0x52,0x52,0x52,0x3E}, // 'g'
	{0x7F,0x08,0x04,0x04,0x78}, // 'h'
	{0x00,0x44,0x7D,0x40,0x00}, // 'i'
	{0x20,0x40,0x44,0x3D,0x00}, // 'j'
	{0x7F,0x10,0x28,0x44,0x00}, // 'k'
	{0x00,0x41,0x7F,0x40,0x00}, // 'l'
	{0x7C,0x04,0x18,0x04,0x78}, // 'm'
	{0x7C,0x08,0x04,0x04,0x78}, // 'n'
	{0x38,0x44,0x44,0x44,0x38}, // 'o'
	{0x7C,0x14,0x14,0x14,0x08}, // 'p'
	{0x08,0x14,0x14,0x18,0x7C}, // 'q'
	{0x7C,0x08,0x04,0x04,0x08}, // 'r'
	{0x48,0x54,0x54,0x54,0x20}, // 's'
	{0x04,0x3F,0x44,0x40,0x20}, // 't'
	{0x3C,0x40,0x40,0x20,0x7C}, // 'u'
	{0x1C,0x20,0x40,0x20,0x1C}, // 'v'
	{0x3C,0x40,0x30,0x40,0x3C}, // 'w'
	{0x44,0x28,0x10,0x28,0x44}, // 'x'
	{0x0C,0x50,0x50,0x50,0x3C}, // 'y'
	{0x44,0x64,0x54,0x4C,0x44}, // 'z'
	{0x00,0x08,0x36,0x41,0x00}, // '{'
	{0x00,0x00,0x7F,0x00,0x00}, // '|'
	{0x00,0x41,0x36,0x08,0x00}, // '}'
	{0x10,0x08,0x08,0x10,0x08}, // '~'
	{0x7F,0x7F,0x7F,0x7F,0x7F}, // 7F DEL (placeholder)
};

SSD1322::SSD1322()
{
	initVars();
}

SSD1322::~SSD1322()
{
}

void SSD1322::init() 
{
	openSPI();
	openGPIO();
	reset();
	initSequence();
	clear();
	update();
}

void SSD1322::close() 
{
	#if defined(PICO_RP2040)
	#else
	if (spi_fd_ >= 0) { ::close(spi_fd_); spi_fd_ = -1; }
	if (line_dc_) { gpiod_line_release(line_dc_);  line_dc_ = nullptr; }
	if (line_rst_) { gpiod_line_release(line_rst_); line_rst_ = nullptr; }
	if (chip_) { gpiod_chip_close(chip_);       chip_ = nullptr; }
	#endif
}

// -- Framebuffer access ----------------------------------------------------

// Clear framebuffer to given 4-bit gray level (0x15)
void SSD1322::clear(uint8_t gray /* = 0 */) 
{
	uint8_t byte = ((gray & 0x0F) << 4) | (gray & 0x0F);
	std::fill(fb_.begin(), fb_.end(), byte);
}

// Set individual pixel, x: 0-255, y: 0-63, gray: 0-15
void SSD1322::setPixel(int x, int y, uint8_t gray) 
{
	if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
	gray &= 0x0F;
	size_t idx = y * (DISPLAY_WIDTH / 2) + x / 2;
	if (x & 1)
		fb_[idx] = (fb_[idx] & 0xF0) | gray;         // low nibble = right pixel
	else
		fb_[idx] = (fb_[idx] & 0x0F) | (gray << 4);  // high nibble = left pixel
}
void SSD1322::aaPixel(int x, int y,uint8_t gray)
{
	if (getPixel(x, y) == gray)
		return;
	bool g1 = getPixel(x - 1, y) == gray;
	bool g2 = getPixel(x, y - 1) == gray;
	bool g3 = getPixel(x + 1, y) == gray;
	bool g4 = getPixel(x, y + 1) == gray;
	if ((g1 && g2) || (g2 && g3) || (g3 && g4) || (g4 && g1))
		setPixel(x, y, gray / 2);
}

void SSD1322::fastAA(int y1, int y2)
{
	int dwh = DISPLAY_WIDTH / 2;
	if (y1 < 0) y1 = 0;
	if (y1 > DISPLAY_HEIGHT) return;
	if (y2 < 0) y2 = DISPLAY_HEIGHT;
	if (y2 > DISPLAY_HEIGHT) y2 = DISPLAY_HEIGHT;
	if (y1 > y2) return;
	int idxStart = y1 * dwh;
	int idxEinde = (y2 + 1) * dwh;
	if (idxEinde > fb_.size()) idxEinde = fb_.size();

	uint8_t grsOld = fb_[idxStart];
	for (int i = idxStart + 1; i < idxEinde; i++)
	{
		uint8_t grsNew = fb_[i];
		if (grsNew == grsOld) continue;
		int y = i / dwh;
		int x = (i % dwh) * 2;
		uint8_t g1 = (grsOld >> 4);
		uint8_t g2 = grsOld & 0xF;
		uint8_t g3 = (grsNew >> 4);
		uint8_t g4 = grsNew & 0xF;
		grsOld = grsNew;
		if ((g1 == 0) && (g2 > 0))	aaPixel(x - 2, y, g2);
		if (g2 == 0)
		{
			if (g1 > 0) aaPixel(x - 1, y, g1);
			else if (g3 > 0) aaPixel(x - 1, y, g3);
		}
		if (g3 == 0)
		{
			if (g2 > 0) aaPixel(x , y, g2);
			else if (g4 > 0) aaPixel(x , y, g4);
		}
		if ((g4 == 0) && (g3 > 0)) aaPixel(x + 1 , y, g3);
	}
}

// Get pixel gray value
uint8_t SSD1322::getPixel(int x, int y) const 
{
	if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return 0;
	size_t idx = y * (DISPLAY_WIDTH / 2) + x / 2;
	return (x & 1) ? (fb_[idx] & 0x0F) : (fb_[idx] >> 4);
}

// Fill rectangle with gray level
void SSD1322::fillRect(int x, int y, int w, int h, uint8_t gray) 
{
	for (int row = y; row < y + h; ++row)
		for (int col = x; col < x + w; ++col)
			setPixel(col, row, gray);
}

// Draw a horizontal line
void SSD1322::hLine(int x, int y, int len, uint8_t gray) 
{
	fillRect(x, y, len, 1, gray);
}

// Draw a vertical line
void SSD1322::vLine(int x, int y, int len, uint8_t gray) 
{
	for (int i = 0; i < len; ++i) setPixel(x, y + i, gray);
}

// Draw unfilled rectangle border
void SSD1322::drawRect(int x, int y, int w, int h, uint8_t gray) 
{
	hLine(x, y, w, gray);
	hLine(x, y + h - 1, w, gray);
	vLine(x, y, h, gray);
	vLine(x + w - 1, y, h, gray);
}

// Draw circle (Bresenham)
void SSD1322::drawCircle(int cx, int cy, int r, uint8_t gray) 
{
	int x = r, y = 0, err = 0;
	while (x >= y) {
		setPixel(cx + x, cy + y, gray); setPixel(cx + y, cy + x, gray);
		setPixel(cx - y, cy + x, gray); setPixel(cx - x, cy + y, gray);
		setPixel(cx - x, cy - y, gray); setPixel(cx - y, cy - x, gray);
		setPixel(cx + y, cy - x, gray); setPixel(cx + x, cy - y, gray);
		if (err <= 0) { ++y; err += 2 * y + 1; }
		if (err > 0) { --x; err -= 2 * x + 1; }
	}
}

int  SSD1322::drawChar(const GFXfont* gfx_font, int x, int y, uint8_t c, uint8_t fg, uint8_t bg /*= 0 */)
{
	if (gfx_font == NULL) return 0;
	if (c < gfx_font->first) return 0;		// we don't have a character for this char
	if (c > gfx_font->last) return 0;		// we don't have a character for this char

	bool bNarrow = (c == '.') || (c == ';') || (c == ',') || (c == ':') || (c == ' ');
	bNarrow = false;

	c -= (uint8_t)gfx_font->first;          //convert input char to corresponding byte from font array
	GFXglyph* glyph = gfx_font->glyph + c;  //get pointer of glyph corresponding to char
	uint8_t* bitmap = gfx_font->bitmap;     //get pointer of char bitmap

	uint16_t bo = glyph->bitmapOffset;
	uint8_t width = glyph->width;
	uint8_t height = glyph->height;

	int8_t x_offset = glyph->xOffset;
	int8_t y_offset = glyph->yOffset;

	uint8_t bit = 0;
	uint8_t bits = 0;

	// this one is 21 for the big font
	if (glyph->xAdvance <= 9) bNarrow = false;

	// for the 18pt font this is 22
	if (bNarrow)  x -= (glyph->xAdvance * 2 / 5);

	for (uint8_t y_pos = 0; y_pos < height; y_pos++)
	{
		for (uint8_t x_pos = 0; x_pos < width; x_pos++)
		{
			if (!(bit++ & 7))
				bits = (*(const unsigned char *)(&bitmap[bo++]));
			if (bNarrow)	// for narrow we only do the on bits not the off bits
			{
				if (bits & 0x80) setPixel(x + x_offset + x_pos, y + y_offset + y_pos,  fg);
			}else
				setPixel(x + x_offset + x_pos, y + y_offset + y_pos, (bits & 0x80) ? fg : bg);
			bits <<= 1;
		}
	}

	//if (height > 6)
	//{
	//	for (uint8_t y_pos = 0; y_pos < height; y_pos++)
	//	{
	//		for (uint8_t x_pos = 0; x_pos < width; x_pos++)
	//		{
	//			aaPixel(x + x_offset + x_pos, y + y_offset + y_pos, fg);
	//		}
	//	}
	//}

	if (bNarrow) return (glyph->xAdvance / 3);
	return glyph->xAdvance;
}


// Draw 5x7 ASCII character (built-in font)
int SSD1322::drawChar(int x, int y, uint8_t c, uint8_t fg, uint8_t bg /* = 0 */)
{
	if (c >= 0xA0) { c &= 0x7F; fg = colorFlash; }
	if (curFontS) return drawChar(curFontS, x, y + curFontShigh, c, fg, bg) + 1;

	if ((c < 0x20) || (c > 0x7F)) c = '?';

	c -= 0x20;
	const uint8_t* glyph = font5x7L[c];
	for (int col = 0; col < 5; ++col) {
		uint8_t bits = glyph[col];
		for (int row = 0; row < 7; ++row)
			setPixel(x + col, y + row, (bits >> row) & 1 ? fg : bg);
	}
	return 6;	// 5 pixels char + 1 pixel space
}

int SSD1322::drawCharB(int x, int y, uint8_t c, uint8_t fg, uint8_t bg /* = 0*/)
{
	if (c >= 0xA0) { c &= 0x7F; fg = colorFlash; }

	if (curFontB)
	{
		int iRes = drawChar(curFontB, x, y + curFontBhigh, c, fg, bg);
		if (iRes > 0) return iRes + 1;
	}

	if ((c < 0x20) || (c > 0x7F)) c = '?';
	c -= 0x20;
	const uint8_t* glyph = font5x7L[c];
	for (int col = 0; col < 5; ++col) {
		uint8_t bits = glyph[col];
		for (int row = 0; row < 7; ++row)
		{
			setPixel(x + col * 2 + 0, y + row * 3 + 0, (bits >> row) & 1 ? fg : bg);
			setPixel(x + col * 2 + 0, y + row * 3 + 1, (bits >> row) & 1 ? fg : bg);
			setPixel(x + col * 2 + 0, y + row * 3 + 2, (bits >> row) & 1 ? fg : bg);
			setPixel(x + col * 2 + 1, y + row * 3 + 0, (bits >> row) & 1 ? fg : bg);
			setPixel(x + col * 2 + 1, y + row * 3 + 1, (bits >> row) & 1 ? fg : bg);
			setPixel(x + col * 2 + 1, y + row * 3 + 2, (bits >> row) & 1 ? fg : bg);
		}
	}
	return 11;	// 10 pixel char + 1 pixel space
}

// Draw null-terminated string
void SSD1322::drawString(int x, int y, const char* str, uint8_t fg, uint8_t bg /* = 0 */,int iHorExtra /* = 0 */)
{
	for (; *str; ++str)
	{
		x += drawChar(x, y, *str, fg, bg);
		x += iHorExtra;
	}
}

void SSD1322::drawString(const GFXfont *font, int x, int y, const char *str, uint8_t fg, uint8_t bg, int iHorExtra)
{
	if (font) y+= (font->yAdvance - 1);	// to make it the same as the bitmap font
	for (; *str; ++str)
	{
		x += drawChar(font,x, y, *str, fg, bg);
		x += iHorExtra;
	}
}

void SSD1322::drawStringB(int x, int y, const char* str, uint8_t fg, uint8_t bg /*= 0*/,int iHorExtra /* = 0 */)
{
	for (; *str; ++str)
	{
		x += drawCharB(x, y, *str, fg, bg);
		x += iHorExtra;
	}
}


// Push framebuffer to display GDDRAM
void SSD1322::update() 
{
	// Column window: SSD1322 columns 0x1C*0x5B map to pixels 0-255
	// (depends on remap; adjust if using a different panel start column)
	sendCmd(CMD_SET_COL_ADDR, 0x1C, 0x5B);
	sendCmd(CMD_SET_ROW_ADDR, 0x00, 0x3F);
	sendCmd(CMD_WRITE_RAM);
	sendData(fb_.data(), fb_.size());
}

// Set display contrast (0-255)
void SSD1322::setContrast(uint8_t val)
{
	sendCmd(CMD_SET_CONTRAST, val);
}

// Sleep / wake
void SSD1322::sleep(bool on)
{
	sendCmd(on ? CMD_SLEEP_ON : CMD_SLEEP_OFF);
}

#if defined(PICO_RP2040)
#else
// GPIO chip (gpiochip0 on most Pi models; gpiochip4 on Pi 5)
static constexpr const char* GPIO_CHIP = "gpiochip0";
static constexpr int PIN_DC  = 25;   // Data/Command
static constexpr int PIN_RST = 24;   // Reset

// SPI device
static constexpr const char* SPI_DEV = "/dev/spidev0.0";
static constexpr uint32_t    SPI_SPEED  = 10000000;  // 10 MHz
static constexpr uint8_t     SPI_MODE = SPI_MODE_0;
static constexpr uint8_t     SPI_BPW = 8;
#endif

// -- Hardware init ---------------------------------------------------------
void SSD1322::openSPI() 
{
	#if defined(PICO_RP2040)
	   // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 10*1000*1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
 	#else
	spi_fd_ = ::open(SPI_DEV, O_RDWR);
	if (spi_fd_ < 0) throw std::runtime_error("Cannot open " + std::string(SPI_DEV));
	uint8_t  mode = SPI_MODE;
	uint8_t  bpw = SPI_BPW;
	uint32_t speed = SPI_SPEED;
	if (ioctl(spi_fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
		ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0 ||
		ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
		throw std::runtime_error("SPI ioctl failed");
	#endif
}

void SSD1322::openGPIO() 
{
	#if defined(PICO_RP2040)

   // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(PIN_CS);
    gpio_init(PIN_RESET);
    gpio_init(PIN_DCA);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
 
    gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_set_dir(PIN_DCA, GPIO_OUT);
    gpio_put(PIN_RESET, 1);
    gpio_put(PIN_DCA, 0);

	#else

	chip_ = gpiod_chip_open_by_name(GPIO_CHIP);
	if (!chip_) throw std::runtime_error("Cannot open GPIO chip");

	line_dc_ = gpiod_chip_get_line(chip_, PIN_DC);
	line_rst_ = gpiod_chip_get_line(chip_, PIN_RST);
	if (!line_dc_ || !line_rst_)
		throw std::runtime_error("Cannot get GPIO lines");

	if (gpiod_line_request_output(line_rst_, "ssd1322_rst", 1) < 0)
		throw std::runtime_error("Cannot configure GPIO RST as output");
	if (gpiod_line_request_output(line_dc_, "ssd1322_dc", 1) < 0)
		throw std::runtime_error("Cannot configure GPIO DC as output");
	#endif
}

void SSD1322::setDC(int v) 
{
	#if defined(PICO_RP2040)
    gpio_put(line_dc_, v);   // drive DATA low to select the device
	#else
	gpiod_line_set_value(line_dc_, v);
	#endif
}
void SSD1322::setRST(int v) 
{
	#if defined(PICO_RP2040)
	gpio_put(line_rst_, v);   // drive RESET low to select the device
	#else
	gpiod_line_set_value(line_rst_, v); 
	#endif
}

void SSD1322::reset() 
{
	setRST(1); sleep_ms(10);
	setRST(0); sleep_ms(10);
	setRST(1); sleep_ms(10);
}

// -- SPI transfer helpers --------------------------------------------------
void SSD1322::spiWrite(const uint8_t* buf, size_t len)
{
	// Chunk transfers for large payloads (kernel spi buf limit ~4096)
	#if defined(PICO_RP2040)
    gpio_put(PIN_CS	, 0);   // drive CS low to select the device
	spi_write_blocking(SPI_PORT, buf, len);
    gpio_put(PIN_CS	, 1);   // drive CS high to release the device
	#else
	static constexpr size_t CHUNK = 4096;
	while (len) {
		size_t n = std::min(len, CHUNK);
		spi_ioc_transfer tr{};
		tr.tx_buf = reinterpret_cast<uintptr_t>(buf);
		tr.len = static_cast<uint32_t>(n);
		tr.speed_hz = SPI_SPEED;
		tr.bits_per_word = SPI_BPW;
		if (ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &tr) < 0)
			throw std::runtime_error("SPI write failed");
		buf += n;
		len -= n;
	}
	#endif
}

// Send one command byte (DC=0) followed by optional parameter bytes
template<typename... Args>
void SSD1322::sendCmd(uint8_t cmd, Args... params)
{
	setDC(0);
	spiWrite(&cmd, 1);
	if constexpr (sizeof...(params) > 0) {
		setDC(1);
		uint8_t p[] = { static_cast<uint8_t>(params)... };
		spiWrite(p, sizeof(p));
	}
	setDC(0);
}

void SSD1322::sendData(const uint8_t* buf, size_t len)
{
	setDC(1);
	spiWrite(buf, len);
	setDC(0);
}

// -- Initialization sequence -----------------------------------------------
void SSD1322::initSequence()
{
	sendCmd(0xFD,0x12);					// unlock

	sendCmd(CMD_SLEEP_ON);              // display off during init

	sendCmd(CMD_SET_CLK_DIV, 0x91);    // clock div; OSC freq bits[7:4]=9, div=2
	sendCmd(CMD_SET_MUX_RATIO, 0x3F);  // 64 MUX (63+1)
	sendCmd(CMD_SET_DISPLAY_OFFSET, 0x00);
	sendCmd(CMD_SET_DISPLAY_START, 0x00);

	// Remap: column address remap, nibble remap, horizontal address increment,
	//        COM remap, split odd/even COM, dual COM line mode
	sendCmd(CMD_SET_REMAP, 0x14, 0x11);
	sendCmd(CMD_SET_GPIO, 0x00);        // GPIO off
	sendCmd(CMD_SET_FUNC_SEL, 0x01);    // internal VDD regulator

	// Display enhancement A
	sendCmd(0xB4, 0xA0, 0xFD);          // VSL external, enhanced low GS quality
	sendCmd(CMD_SET_CONTRAST, 0xCF);    // contrast
	//sendCmd(CMD_SET_CONTRAST, 0x7F);    // contrast
	sendCmd(CMD_MASTER_CONTRAST, 0x0F); // master contrast max
	sendCmd(CMD_DEFAULT_GRAY_SCALE);    // use default linear gray scale table
	sendCmd(CMD_SET_PHASE_LEN, 0xE2);   // phase 1=5, phase 2=14 DCLKs
	// Display enhancement B
	sendCmd(0xD1, 0x82, 0x20);
	sendCmd(CMD_SET_PRECHARGE_V, 0x1F);
	sendCmd(CMD_SET_PRECHARGE2, 0x08); // 2nd pre-charge = 8 DCLKs
	sendCmd(CMD_SET_VCOMH, 0x07); // VCOM = 0.86 � VCC
	//sendCmd(CMD_DISPLAY_NORMAL);        // normal (non-inverted) display
	sendCmd(CMD_DISPLAY_ALL_OFF);
	sendCmd(CMD_SLEEP_OFF);             // display on
	
	//usleep(10);
}


#endif // __linux__






