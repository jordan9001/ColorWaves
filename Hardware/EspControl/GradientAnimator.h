#ifndef GRADIENTANIMATOR_H
#define GRADIENTANIMATOR_H

#include <stdint.h>
#include <Arduino.h>

#define MAX_POINTS  0x20
#ifndef NULL
#define NULL        0
#endif

class GradientAnimator {
	public:
		GradientAnimator(void);
		~GradientAnimator(void);

		void clear(void);
		bool addPoint(uint8_t node, uint32_t color, uint16_t index, int32_t msoff);
		bool getGradient(uint32_t* colors, uint16_t len, int32_t ms);
		void setLooptime(int32_t ms);
		int32_t getLooptime();
		bool parseBuffer(uint8_t* buf, uint16_t sz);
		void printDebug();
	
	private:
		struct point {
			uint32_t color;
			uint16_t index;
			int32_t msoff;
			uint8_t node;

			struct point* next;
		};

		uint32_t color_lerp(uint32_t ac, uint32_t bc, uint16_t index, uint16_t len);
		uint8_t byte_lerp(uint8_t a, uint8_t b, uint16_t i, uint16_t len);

		uint32_t looptime;
		uint8_t num_nodes;
		struct point* head;

		uint16_t max_point_i;
		struct point point_mem[MAX_POINTS];
};

#endif
