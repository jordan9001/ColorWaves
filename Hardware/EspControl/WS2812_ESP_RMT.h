#ifndef WS2812ESPRMT_H
#define WS2812ESPRMT_H

#include <stdint.h>
#include <Arduino.h>
#include <driver/rmt.h>

class ESPRMTLED {
	public:
		ESPRMTLED(uint16_t numleds, uint8_t pin, uint16_t rmt_chan);
		~ESPRMTLED(void);

		void show();
		void clear();
		void setPixelColor(uint16_t i, uint32_t color);

	private:
		uint16_t numpixels;
		uint16_t numbits;
		rmt_channel_t channel;
		rmt_item32_t* bits;
		uint32_t* colors;
		rmt_item32_t highbit;
		rmt_item32_t lowbit;
		rmt_item32_t termbit;
};

#endif