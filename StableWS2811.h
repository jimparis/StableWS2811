#ifndef STABLEWS2811_H
#define STABLEWS2811_H

/* StableWS2811 - Jitter-free WS2811 LED Display Library
   Copyright (c) 2014 Jim Paris
   Copyright (c) 2013 Paul Stoffregen, PJRC.COM, LLC

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <stdint.h>

#define WS2811_RGB	0	// The WS2811 datasheet documents this way
#define WS2811_RBG	1
#define WS2811_GRB	2	// Most LED strips and WS2812 are wired this way
#define WS2811_GBR	3
#define WS2811_COLOR_MASK 0x0f

#define WS2811_800kHz 0x00	// Nearly all WS2811 are 800 kHz
#define WS2811_400kHz 0x10	// Adafruit's Flora Pixels
#define WS2811_FREQ_MASK 0x10

class StableWS2811 {
public:
	/* pixelBuf must be (stripLen * 3) bytes
           spiBuf must be (stripLen * 6) words, aligned in DMAMEM
           e.g.
           static DMAMEM uint32_t spiBuf[STRIP_LEN * 6];
           static uint8_t pixelBuf[STRIP_LEN * 3];
	*/
	StableWS2811(uint16_t stripLen, uint32_t *spiBuf, uint8_t *pixelBuf,
                     uint8_t config = WS2811_GRB | WS2811_800kHz);
	void begin(void);
        void end(void);

	void setPixel(uint32_t num, int color);
	void setPixel(uint32_t num, uint8_t red, uint8_t green, uint8_t blue) {
		setPixel(num, color(red, green, blue));
	}
	int getPixel(uint32_t num);

	void show(void);
	int busy(void);

	int numPixels(void) {
		return stripLen;
	}
	int color(uint8_t red, uint8_t green, uint8_t blue) {
		return (red << 16) | (green << 8) | blue;
	}

private:
	uint16_t stripLen;
	uint32_t *spiBuf;
	uint8_t *pixelBuf;
	uint8_t config;
};

#endif
