#pragma once
#include "common.h"
#include "VBoxCAPIGlue.h"
#include "vbox_helper.h"

// Initialize VBox passive events (starts a new event handler thread)
int init_vbox_events(ISession* session);

// Free/cleanup VBox passive events
int free_vbox_events(void);
