#pragma once
#include "common.h"

// Common touch signal struct
#define TOUCH_SIG_STRFMT "%u %u %hhu %d %hhd\n"
struct touch_sig {
	uint32_t x;
	uint32_t y;
	uint8_t slot;
	int32_t tracking_id;
	int8_t touch_state; // -1: doesnt change state / still touching, 0: touch release, 1: initial touch
};

// Modify given buf to hold touch_sig fields in string format delimited by spaces and terminated with \0
// Returns 0 if struct write succeeded, -1 if encoding error, 1 if buf/maxsize was too small
static int serialize_touch_sig(struct touch_sig tsig, char* bufptr, size_t maxsize) {
	int ret = snprintf(bufptr, maxsize, TOUCH_SIG_STRFMT, tsig.x, tsig.y, tsig.slot, tsig.tracking_id, tsig.touch_state);
	if (ret < 0) {
		return -1;
	}
	else if (ret < maxsize) {
		return 0;
	}
	else {
		return 1;
	}
}

// Deserialize touch sig string fields into touch_sig struct
// Returns 0 if deserialization succeeded, -1 if input failure, 1 if couldn't scan all touch_sig fields
static int deserialize_touch_sig(struct touch_sig* tsig, char* bufptr) {
	int ret = sscanf(bufptr, TOUCH_SIG_STRFMT, &(tsig->x), &(tsig->y), &(tsig->slot), &(tsig->tracking_id), &(tsig->touch_state));
	if (ret < 0) {
		return -1;
	}
	else if (ret != 5) {
		return 1;
	}
	else {
		return 0;
	}
}