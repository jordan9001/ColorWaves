#include "GradientAnimator.h"

#define USE_SERIAL		Serial

#define LOOPTIME_T		int32_t
#define POINTCOUNT_T	uint16_t
#define PT_COLOR_T		uint32_t
#define PT_MSOFF_T		int32_t
#define PT_INDEX_T		uint16_t

#define POINT_LEN		(sizeof(PT_COLOR_T) + sizeof(PT_MSOFF_T) + sizeof(PT_INDEX_T))
#define MIN_BUFFER_LEN	(sizeof(LOOPTIME_T) + sizeof(POINTCOUNT_T) + POINT_LEN)

GradientAnimator::GradientAnimator(void) {
	this->clear();
	return;
}

GradientAnimator::~GradientAnimator(void) {
	return;
}

void GradientAnimator::clear(void) {
	num_nodes = 0;
	head = NULL;
	looptime = 0;
	max_point_i = 0;
	return;
}

bool GradientAnimator::addPoint(uint8_t node, uint32_t color, uint16_t index, int32_t msoff) {
	max_point_i++;
	if (max_point_i >= MAX_POINTS) {
		max_point_i--;
		return false;
		// USED UP TOO MANY POINTS
	}

	if (node >= num_nodes) {
		// no checks to make sure they don't skip a node
		num_nodes = node+1;
	}

	point_mem[max_point_i].node = node;
	point_mem[max_point_i].color = color;
	point_mem[max_point_i].index = index;
	point_mem[max_point_i].msoff = msoff;
	point_mem[max_point_i].next = NULL;

	// find best spot to insert into linked list
	struct point* prev = NULL;
	struct point* cur = head;

	if (head == NULL) {
		head = &point_mem[max_point_i];
		return true;
	}

	while (cur != NULL && cur->msoff < msoff) {
		prev = cur;
		cur = cur->next;
	}

	if (cur == NULL) {
		// end of line
		prev->next = &point_mem[max_point_i];
	} else if (cur == head) {
		// up front
		point_mem[max_point_i].next = cur;
		head = &point_mem[max_point_i];
	} else {
		// spot in middle
		point_mem[max_point_i].next = cur;
		prev->next = &point_mem[max_point_i];
	}

	return true;
}

bool GradientAnimator::getGradient(uint32_t* colors, uint16_t len, int32_t ms) {
	// Ok, so we clear the colors, setting their 0x80000000 bit.
	// Then we go through and fill in between the nodes
	if (looptime == 0) {
		return false;
	}

	uint16_t i;
	for (i=0; i<len; i++) {
		colors[i] = 0x80000000;
	}
	// for each node, get it's color and index at this offset, and start filling in the gradient
	uint8_t n;
	struct point* a;    // a is the point in this node earlier than this point
	int32_t ad;
	struct point* b;    // b is the point in this node later than this point
	int32_t bd;
	struct point* t;
	int32_t td;
	uint32_t lerp_color;
	uint16_t lerp_index;

	for (n=0; n<num_nodes; n++) {
		a = NULL;
		b = NULL;
		//USE_SERIAL.printf("Finding lerp node for node 0x%x\n", n);
		for (t = head; t != NULL; t = t->next) {
			if (t->node != n) {
				continue;
			}

			//USE_SERIAL.printf("\tNode 0x%x: #%06x 0x%x 0x%x/0x%x\n", t->node, t->color, t->index, t->msoff, looptime);

			td = (ms - t->msoff);
			while (td < 0) {
				td += looptime;
			} // wrap around distance
			if (a == NULL || (td <= ad)) {
				a = t;
				ad = td;
				//USE_SERIAL.printf("\t\tChoosen as previous node\n");
			}
			td = looptime - td;
			if (b == NULL || (td < bd)) {
				b = t;
				bd = td;
				//USE_SERIAL.printf("\t\tChoosen as next node\n");
			}
		}
		if (a == NULL || b == NULL) {
			return false;
		}

		// interpolate the point
		lerp_index = (uint16_t)(((((int32_t)b->index - (int32_t)a->index) * (ad)) / (ad + bd)) + (int32_t)a->index);
		lerp_color = color_lerp(a->color, b->color, ad, ad + bd);

		//USE_SERIAL.printf("For node 0x%x, found color #%x at index 0x%x\n", n, lerp_color, lerp_index);
		// if there is a collision, try bumping the other one right or left
		//TODO

		colors[lerp_index] = lerp_color;
	}

	// fill in the rest of the stuff
	uint16_t ai = 0;
	uint16_t bi = 0;
	uint16_t j;
	for (i=0; i<len; i++) {
		if (colors[i] != 0x80000000) {
			ai = i;
			continue;
		}
		if (bi <= i) {
			// track forward to find the next to lerp to
			bi = len - 1;
			for (j=i+1; j<len; j++) {
				if (colors[j] != 0x80000000) {
					bi = j;
					break;
				}
			}
		}

		if (colors[ai] == 0x80000000) {
			// case where we are at the start, and undefined
			colors[i] = colors[bi];
		} else if (colors[bi] == 0x80000000) {
			// case where the end is undefined
			colors[i] = colors[ai];
		} else {
			// lerp the two
			colors[i] = color_lerp(colors[ai], colors[bi], i-ai, bi-ai);
		}
	}
	
	return true;
}

void GradientAnimator::setLooptime(int32_t ms) {
	looptime = ms;
}

int32_t GradientAnimator::getLooptime() {
	return looptime;
}

uint32_t GradientAnimator::color_lerp(uint32_t ac, uint32_t bc, uint16_t index, uint16_t len) {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	if (len == 0) {
		return ac;
	}
	r = byte_lerp((uint8_t)(ac >> 16), (uint8_t)(bc >> 16), index, len);
	g = byte_lerp((uint8_t)(ac >> 8), (uint8_t)(bc >> 8), index, len);
	b = byte_lerp((uint8_t)(ac), (uint8_t)(bc), index, len);
	return (r<<16) | (g<<8) | b;
}

uint8_t GradientAnimator::byte_lerp(uint8_t a, uint8_t b, uint16_t i, uint16_t len) {
	return (uint8_t)(((((int16_t)b-(int16_t)a) * i) / len) + a);
}

bool GradientAnimator::parseBuffer(uint8_t* buf, uint16_t sz) {
	if (sz < MIN_BUFFER_LEN) {
		USE_SERIAL.printf("Bad buffer size passed in, only 0x%x bytes\n", sz);
		return false;
	}
	this->clear();

	uint8_t* cur;
	LOOPTIME_T lpt;
	POINTCOUNT_T pointcount;
	POINTCOUNT_T i;
	PT_COLOR_T p_c;
	PT_MSOFF_T p_m;
	PT_INDEX_T p_i;
	uint8_t node_i = 0;

	cur = buf;
	lpt = *((LOOPTIME_T*)cur);
	USE_SERIAL.printf("Got looptime as %d\n", lpt);
	this->setLooptime(lpt);
	cur += sizeof(LOOPTIME_T);
	while (cur < (buf + sz)) {
		pointcount = *((POINTCOUNT_T*)cur);
		cur += sizeof(POINTCOUNT_T);

		if ((cur + (pointcount * POINT_LEN)) > (buf + sz)) {
			USE_SERIAL.printf("Pointcount would send up past end of buffer. 0x%x\n", pointcount);
			return false;
		}

		USE_SERIAL.printf("Node %d has %d points\n", node_i, pointcount);

		for (i=0; i<pointcount; i++) {
			p_c = *((PT_COLOR_T*)cur);
			cur += sizeof(PT_COLOR_T);
			p_m = *((PT_MSOFF_T*)cur);
			cur += sizeof(PT_MSOFF_T);
			p_i = *((PT_INDEX_T*)cur);
			cur += sizeof(PT_INDEX_T);

			if (!this->addPoint(node_i, p_c, p_i, p_m)) {
				USE_SERIAL.println("Used up too many points!");
				return false;
			}
		}
		node_i++;
	}

	this->printDebug();
	return true;
}

void GradientAnimator::printDebug() {
	struct point* c;

	USE_SERIAL.printf("The loop time is: 0x%x\n", looptime);
	USE_SERIAL.printf("There are 0x%x nodes and 0x%x points\n", num_nodes, max_point_i);

	for (c = head; c != NULL; c = c->next) {
		USE_SERIAL.printf("\tNode 0x%x: #%06x 0x%x 0x%x/0x%x\n", c->node, c->color, c->index, c->msoff, looptime);
	}
}