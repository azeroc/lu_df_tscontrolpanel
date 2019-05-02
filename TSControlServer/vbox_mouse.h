#pragma once
#include "common.h"
#include "vbox_helper.h"
#include "touch_sig.h"

// Defines
#define CONTACT_DATA_SIZE 50

// Initialize VBox IMouse interface objects to allow mouse/touch input actions
int init_vbox_mouse(ISession* session);

// Handle touch_sig structure and emulate touch signal in VM
int vbox_handle_touchsig(struct touch_sig tsig);

// Cleanup VBox IMouse interface objects
int free_vbox_mouse(void);
