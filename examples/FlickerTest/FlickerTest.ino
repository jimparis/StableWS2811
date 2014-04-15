/* StableWS2811 FlickerTest.ino - RGB LED Test
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

   Required Connections
   --------------------
   pin 7:  LED strip data  (OctoWS2811 adapter output #3, green wire)

   This test is useful for checking if your LED strips work, and to
   verify that there is no flickering.  All LEDs will be lit at
   minimum brightness, with brighter colored pixels moving along the
   strip, and the strip will be updated as quickly as possible.  */

#include <StableWS2811.h>

const int stripLen = 120;
const int config = WS2811_GRB | WS2811_800kHz;

static DMAMEM uint32_t spiBuf[stripLen * 6];
static uint8_t pixelBuf[stripLen * 3];

StableWS2811 leds(stripLen, spiBuf, pixelBuf, config);

void setup() {
        leds.begin();
        leds.show();
}

elapsedMillis moveElapsed;
int moveLoc = 0;

void loop()
{
        if (moveElapsed > 50) {
                moveElapsed -= 50;
                moveLoc++;
                if (moveLoc >= leds.numPixels())
                        moveLoc = 0;
        }

        for (int i = 0; i < leds.numPixels(); i++)
                leds.setPixel(i, 0x010101);

        /* Red pixel in front, green in middle, blue trailing.  These
           are kept at a low brightness to keep power usage down, so
           that any flickering is more likely to be caused by software
           problems. */
        leds.setPixel((moveLoc + 10) % leds.numPixels(), 0x110000);
        leds.setPixel((moveLoc + 5)  % leds.numPixels(), 0x001100);
        leds.setPixel((moveLoc + 0)  % leds.numPixels(), 0x000011);

        leds.show();
}
