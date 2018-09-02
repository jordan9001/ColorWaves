#include "GradientAnimator.h"

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
	struct point* a = NULL;    // a is the point in this node earlier than this point
	int32_t ad;
	struct point* b = NULL;    // b is the point in this node later than this point
	int32_t bd;
	struct point* t;
	int32_t td;
	uint32_t lerp_color;
	uint16_t lerp_index;
	for (n=0; n<num_nodes; n++) {
		for (t = head; t != NULL; t = t->next) {
			if (t->node != n) {
				continue;
			}

			td = (ms - t->msoff);
			while (td < 0) {
				td += looptime;
			} // wrap around distance
			if (a == NULL || (td <= ad)) {
				a = t;
				ad = td;
			}
			td = looptime - td;
			if (b == NULL || (td < bd)) {
				b = t;
				bd = td;
			}
		}
		if (a == NULL || b == NULL) {
			return false;
		}

		// interpolate the point
		lerp_index = (uint16_t)(((((int32_t)b->index - (int32_t)a->index) * (ad)) / (ad + bd)) + (int32_t)a->index);
		lerp_color = color_lerp(a->color, b->color, ad, ad + bd);

		// if there is a collision, try bumping the other one right or left
		//TODO
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

	r = (ac & 0xff0000);
	r = (((((bc & 0xff0000) - r) * index) / len) + r) & 0xff0000;
	g = (ac & 0xff00);
	g = (((((bc & 0xff00) - g) * index) / len) + g) & 0xff00;
	b = (ac & 0xff0000);
	b = (((((bc & 0xff0000) - b) * index) / len) + b) & 0xff;
	return r + g + b;
}
