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

#include <Arduino.h>
#include <string.h>
#include "StableWS2811.h"

#define SPI_PUSHR_DATA(x) (SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (x))

static DMAMEM uint32_t zero; /* note: can't initialize DMAMEM variables here */
static volatile uint8_t update_in_progress = 0;
static uint32_t update_completed_at = 0;

/* Each pixel contains 3 bytes:
   RRRRrrrrGGGGggggBBBBbbbb

   We split this into 6 words (24 bytes) that will be written to the
   SPI0.PUSHR register as follows:

     1000 0000 0000 0000 0000 1R01 R01R 01R0
     1000 0000 0000 0000 0000 1r01 r01r 01r0
     1000 0000 0000 0000 0000 1G01 G01G 01G0
     1000 0000 0000 0000 0000 1g01 g01g 01g0
     1000 0000 0000 0000 0000 1B01 B01B 01B0
     1000 0000 0000 0000 0000 1b01 b01b 01b0
     ------------------- ++++ ==============

   Only the low 12 bits (==) get written via SPI.
   The next 4 bits (++) are ignored.
   The upper 16 bits (--) are required because SPI0.PUSHR is stupid,
   and only supports 32-bit writes that contain configuration data.
   The reference manual suggests that a 16 bit write could be used, but
   it's wrong: https://community.freescale.com/message/378836#378836

   That's why we need so much memory:
   pixelBuf should be stripLen*3 bytes
   spiBuf should be stripLen*6 words, or stripLen*24 bytes (aligned).

   It may be possible to reduce this with clever DMA chaining, but it's
   not clear.
*/
StableWS2811::StableWS2811(uint16_t stripLenMax, uint32_t *spiBuf,
                           uint8_t *pixelBuf, uint8_t config)
{
        this->stripLenMax = stripLenMax;
        this->stripLen = stripLenMax;
        this->spiBuf = spiBuf;
        this->pixelBuf = pixelBuf;
        this->config = config;
}

/* Update strip length.  New strip len must be less than or equal to the
   length provided in the constructor, and should only be changed
   before begin() or after end(). */
void StableWS2811::setStripLen(uint16_t newStripLen)
{
        if (newStripLen > stripLenMax)
                stripLen = stripLenMax;
        else
                stripLen = newStripLen;
}

void StableWS2811::begin(void)
{
        int i;
        int pixel_bufsize_bytes = stripLen * 3;
        int spi_bufsize_bytes = stripLen * 24;
        int spi_bufsize_words = stripLen * 6;

        // Set up buffers
        memset(pixelBuf, 0, pixel_bufsize_bytes);
        for (i = 0; i < spi_bufsize_words; i++)
                spiBuf[i] = SPI_PUSHR_DATA(0);
        zero = SPI_PUSHR_DATA(0);

        // Enable clocks to SPI0, DMA, and DMAMUX controllers
        SIM_SCGC6 |= SIM_SCGC6_SPI0 | SIM_SCGC6_DMAMUX;
        SIM_SCGC7 |= SIM_SCGC7_DMA;

	// Set up SPI0 in continuous SCK mode, 12 bit frames, and clock
        // at 3 * WS2811 frequency.
        SPI0.MCR = SPI_MCR_MSTR |
                SPI_MCR_CONT_SCKE |
                SPI_MCR_PCSIS(0x1F) |
                SPI_MCR_MDIS |
                SPI_MCR_HALT;

        uint32_t ctar = SPI_CTAR_FMSZ(11) | SPI_CTAR_CPHA;
#if F_BUS == 48000000
        if ((config & WS2811_FREQ_MASK) == WS2811_400kHz)
                // (48 Mhz / 5) * (1 + 0 / 8) = 1.2 MHz
                ctar |= SPI_CTAR_PBR(2) | SPI_CTAR_BR(3);
        else
                // (48 Mhz / 5) * (1 + 0 / 4) = 2.4 MHz
                ctar |= SPI_CTAR_PBR(2) | SPI_CTAR_BR(1);
#elif F_BUS == 24000000
        if ((config & WS2811_FREQ_MASK) == WS2811_400kHz)
                // (24 Mhz / 5) * (1 + 0 / 4) = 1.2 MHz
                ctar |= SPI_CTAR_PBR(2) | SPI_CTAR_BR(1);
        else
                // (24 Mhz / 5) * (1 + 0 / 2) = 2.4 MHz
                ctar |= SPI_CTAR_PBR(2) | SPI_CTAR_BR(0);
#else
        #error Unsupported F_BUS
#endif
        SPI0.CTAR0 = ctar;

        // Map SPI0 MOSI to pin 7 and enable module
        CORE_PIN7_CONFIG = PORT_PCR_MUX(2);
        SPI0.MCR &= ~(SPI_MCR_HALT | SPI_MCR_MDIS);

        // Configure DMA
	DMA_CR = 0;
	DMA_ERQ = 0;

	// DMA channel #1 copies SPI data, triggers ISR when done
        DMA_TCD1_SADDR = spiBuf;
        DMA_TCD1_SOFF = 4;
        DMA_TCD1_ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	DMA_TCD1_NBYTES_MLNO = 4;
        DMA_TCD1_SLAST = -spi_bufsize_bytes;
        DMA_TCD1_DADDR = &SPI0_PUSHR;
	DMA_TCD1_DOFF = 0;
        DMA_TCD1_CITER_ELINKNO = spi_bufsize_words;
	DMA_TCD1_BITER_ELINKNO = spi_bufsize_words;
	DMA_TCD1_DLASTSGA = 0;
	DMA_TCD1_CSR = DMA_TCD_CSR_DREQ | DMA_TCD_CSR_INTMAJOR;

	// DMA channel #2 always writes zeros
	DMA_TCD2_SADDR = &zero;
	DMA_TCD2_SOFF = 0;
	DMA_TCD2_ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
	DMA_TCD2_NBYTES_MLNO = 4;
	DMA_TCD2_SLAST = 0;
	DMA_TCD2_DADDR = &SPI0_PUSHR;
	DMA_TCD2_DOFF = 0;
	DMA_TCD2_CITER_ELINKNO = 1;
	DMA_TCD2_BITER_ELINKNO = 1;
	DMA_TCD2_DLASTSGA = 0;
	DMA_TCD2_CSR = 0;

        // Give DMA channel #1 priority over #2
        DMA_DCHPRI1 = 2;
        DMA_DCHPRI2 = 1;

	// Route the SPI0 transmit DMA request to both channels
	DMAMUX0_CHCFG1 = 0;
	DMAMUX0_CHCFG1 = DMAMUX_SOURCE_SPI0_TX | DMAMUX_ENABLE;
	DMAMUX0_CHCFG2 = 0;
	DMAMUX0_CHCFG2 = DMAMUX_SOURCE_SPI0_TX | DMAMUX_ENABLE;

	// Enable interrupt when channel #1 completes
	NVIC_ENABLE_IRQ(IRQ_DMA_CH1);

        // Enable DMA channel #2 to send zeros when SPI needs them
        DMA_SERQ = 2;

        // Enable SPI0 TFFF DMA requests
        SPI0.RSER = SPI_RSER_TFFF_RE | SPI_RSER_TFFF_DIRS;
}

void StableWS2811::end(void)
{
        // Wait for current update to finish
        while (update_in_progress)
                continue;

        noInterrupts();
        update_in_progress = 0;

        // Stop DMA
        DMA_TCD2_CSR = DMA_TCD_CSR_DREQ;
        while ((DMA_TCD2_CSR & DMA_TCD_CSR_DONE) == 0)
                continue;

        // Disable everythign

        SPI0.SR = SPI_SR_TFFF;
        DMA_CERQ = 1;
        DMA_CERQ = 2;
        SPI0.RSER = 0;
        NVIC_DISABLE_IRQ(IRQ_DMA_CH1);
        DMAMUX0_CHCFG1 = 0;
        DMAMUX0_CHCFG2 = 0;
        DMA_CR = 0;
        DMA_ERQ = 0;
        CORE_PIN7_CONFIG = PORT_PCR_MUX(1);
        SPI0.MCR = 0;
        SIM_SCGC6 &= ~(SIM_SCGC6_SPI0 | SIM_SCGC6_DMAMUX);
        SIM_SCGC7 &= ~(SIM_SCGC7_DMA);

        interrupts();
}

void dma_ch1_isr(void)
{
	DMA_CINT = 1;
	update_completed_at = micros();
	update_in_progress = 0;
}

int StableWS2811::busy(void)
{
	if (update_in_progress)
                return 1;
	// Busy for 50 us after spiBuf is transferred, for WS2811 reset
	if (micros() - update_completed_at < 50)
                return 1;
	return 0;
}

/* Map a 4 bit nibble into a 12 bit value to be written to the SPI bus */
static const uint16_t led_spi_lookup[16] = {
        // a 0 bit should be sent as 0b100, or 4 octal
        // a 1 bit should be sent as 0b110, or 6 octal
        04444, 04446, 04464, 04466, 04644, 04646, 04664, 04666,
        06444, 06446, 06464, 06466, 06644, 06646, 06664, 06666
};

void StableWS2811::show(void)
{
        int i;

	// Wait for prior DMA operations to complete
        while (update_in_progress)
                continue;

        // Convert pixel buffer into SPI buffer
        uint8_t *pix = pixelBuf;
        uint32_t *spi = spiBuf;
        for (i = 0; i < stripLen * 3; i++) {
                *spi++ = SPI_PUSHR_DATA(led_spi_lookup[*pix >> 4]);
                *spi++ = SPI_PUSHR_DATA(led_spi_lookup[*pix & 0xf]);
                pix++;
        }

        // Wait for WS2811 reset
	while (micros() - update_completed_at < 50)
                continue;

        noInterrupts();
	update_in_progress = 1;

        // Ensure DMA channel #2 has stopped
        DMA_TCD2_CSR = DMA_TCD_CSR_DREQ;
        while ((DMA_TCD2_CSR & DMA_TCD_CSR_DONE) == 0)
                continue;

        // Make sure SPI hardware's TFFF flag updates correctly.
        // Sometimes the last DMA transfer from channel #2 doesn't do
        // this.
        SPI0.SR = SPI_SR_TFFF;

        // Enable both DMA channels
	DMA_TCD2_CSR = 0;
        DMA_SERQ = 1;
        DMA_SERQ = 2;
        interrupts();
}

void StableWS2811::setPixel(uint32_t num, int color)
{
        uint8_t *pix = &((uint8_t *)pixelBuf)[num * 3];

#define SET_RGB(r,g,b) do {                        \
                pix[r] = (color & 0xFF0000) >> 16; \
                pix[g] = (color & 0xFF00) >> 8;    \
                pix[b] = (color & 0xFF);           \
        } while(0)

        switch (config & WS2811_COLOR_MASK)
        {
        case WS2811_RGB:
                SET_RGB(0, 1, 2);
                break;
        case WS2811_RBG:
                SET_RGB(0, 2, 1);
                break;
        case WS2811_GRB:
                SET_RGB(1, 0, 2);
                break;
        case WS2811_GBR:
                SET_RGB(1, 2, 0);
                break;
        default:
                break;
        }
}

int StableWS2811::getPixel(uint32_t num)
{
        uint8_t *pix = &((uint8_t *)pixelBuf)[num * 3];

#define GET_RGB(r,g,b) ((pix[r] << 16) | (pix[g] << 8) | pix[b])

        switch (config & WS2811_COLOR_MASK)
        {
        case WS2811_RGB:
                return GET_RGB(0, 1, 2);
        case WS2811_RBG:
                return GET_RGB(0, 2, 1);
        case WS2811_GRB:
                return GET_RGB(1, 0, 2);
        case WS2811_GBR:
                return GET_RGB(1, 2, 0);
        default:
                return 0;
        }
}
