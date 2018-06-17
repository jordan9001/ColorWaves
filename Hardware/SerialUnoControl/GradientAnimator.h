#ifndef GRADIENTANIMATOR_H
#define GRADIENTANIMATOR_H

#include <Arduino.h>

#define MAX_POINTS  0x100

class GradientAnimator {
    public:
        GradientAnimator(void);
        ~GradientAnimator(void);

        void clear(void);
        bool addPoint(uint8_t node, uint32_t color, uint16_t index, int32_t msoff);
        bool getGradient(uint32_t* colors, uint16_t len, int32_t ms);
        void setLooptime(uint32_t ms);
    
        private:
        struct point {
            uint32_t color;
            uint16_t index;
            int32_t msoff;
            uint8_t node;

            struct point* next;
        }
        uint32_t color_lerp(uint32_t ac, uint32_t bc, uint16_t index, uint_16_t len);

        uint32_t looptime;
        uint8_t num_nodes;
        struct point* head;

        uint16_t max_point_i;
        struct point point_mem[MAX_POINTS];
}

#endif