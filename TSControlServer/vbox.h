#pragma once
#include "common.h"
#include "touch_sig.h"
#include "VBoxCAPIGlue.h"

// === TSControlServer VBox API access layer methods ===

// Initialize VBox objects
int init_vbox(void);

// Destroy/free VBox objects
void free_vbox(void);
