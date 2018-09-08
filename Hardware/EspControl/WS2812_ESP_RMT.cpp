#include "WS2812_ESP_RMT.h"

// The esp32-snippets by nkolban were super helpful here!
//I just rewrote it to better understand

ESPRMTLED::ESPRMTLED(uint16_t numleds, uint8_t pin, uint16_t rmt_chan) {
	this->numpixels = numleds;
	this->numbits = (numleds * 24) + 1;
	this->channel = (rmt_channel_t)channel;

	// I need 24 bits per led, plus a terminator
	this->bits = new rmt_item32_t[this->numbits];
	this->colors = new uint32_t[numleds];
	this->clear();

	pinMode(pin, OUTPUT);

	/*
	 *	typedef struct {
	 *		union {
	 *			struct {
	 *				uint32_t duration0 :15;
	 *				uint32_t level0 :1;
	 *				uint32_t duration1 :15;
	 *				uint32_t level1 :1;
	 *			};
	 *			uint32_t val;
	 *		};
	 *	} rmt_item32_t;
	 */
	// all the timing is done here.
	// the clk_div is 8, so we are working with 100ns units in duration
	this->highbit.duration0 = 8; // should be 0.8 us
	this->highbit.level0 = 1;
	this->highbit.duration1 = 5; // shouod be .45 us, but we have .15 wiggle room
	this->highbit.level1 = 0;

	this->lowbit.duration0 = 4;	// should be 0.4 us
	this->lowbit.level0 = 1;
	this->lowbit.duration1 = 8; // should be 0.85 us, but we have .15 us wiggle room
	this->lowbit.level1 = 0;

	this->termbit.val = 0;


	rmt_config_t config;
	config.rmt_mode = RMT_MODE_TX;
	config.channel = this->channel;
	config.gpio_num = (gpio_num_t)pin;
	// number of memory blocks used for now just take as many as we can get
	config.mem_block_num = 8 - rmt_chan;
	// range of pulse len generated. Source clock is typically 80Mhz.
	// So div by 8 means each tick is at 100ns
	config.clk_div = 8;
	config.tx_config.loop_en = 0;
	config.tx_config.carrier_en      = 0;
	config.tx_config.idle_output_en  = 1;
	config.tx_config.idle_level      = (rmt_idle_level_t)0;
	// we disabled the carrier, but fill it out anyway
	config.tx_config.carrier_freq_hz = 10000;
	config.tx_config.carrier_level   = (rmt_carrier_level_t)1;
	config.tx_config.carrier_duty_percent = 50;

	rmt_config(&config);
	// no rx buf, default flags
	rmt_driver_install(this->channel, 0, 0);
}

ESPRMTLED::~ESPRMTLED(void) {
	delete[] this->colors;
	delete[] this->bits;
}

void ESPRMTLED::show() {
	rmt_item32_t* cur;
	uint16_t i;
	int bt;
	uint32_t c;
	bool bitset;

	cur = this->bits;

	for (i=0; i<this->numpixels; i++) {
		c = this->colors[i];

		// ok we have the color, we need to set up our bits from MSB to LSB in GRB



		// set up G
		for (bt = 15; bt >= 8; bt--) {
			bitset = c & (1 << bt);
			*cur = (bitset) ? this->highbit : this->lowbit;
			cur++;
		}
		// set up R
		for (bt = 23; bt >= 16; bt--) {
			bitset = c & (1 << bt);
			*cur = (bitset) ? this->highbit : this->lowbit;
			cur++;
		}
		// set up B
		for (bt = 7; bt >= 0; bt--) {
			bitset = c & (1 << bt);
			*cur = (bitset) ? this->highbit : this->lowbit;
			cur++;
		}

	}
	// set terminator
	*cur = this->termbit;

	// tell the rmt to write it out
	rmt_write_items(this->channel, this->bits, this->numbits, 1); // wait_tx_done
}

void ESPRMTLED::clear() {
	uint16_t i;
	for (i=0; i<this->numpixels; i++) {
		this->colors[i] = 0;
	}
}

void ESPRMTLED::setPixelColor(uint16_t i, uint32_t color) {
	if (i > this->numpixels) {
		return;
	}
	// Store GRB
	//this->colors[i] = ((color & 0xff0000) >> 8) | ((color & 0xff00) << 8) | (color & ff);
	this->colors[i] = color;
}