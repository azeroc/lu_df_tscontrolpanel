#pragma once
#include "common.h"
#include "touch_sig.h"
#include "vbox_mouse.h"

#define READBUF_SIZE 1024
#define MAXCONNS 128

// Starts VBox touch signal server, uses select polling on sockets instead of usual new-connection-new-thread pattern
int start_tsserver(uint16_t listenPort);
