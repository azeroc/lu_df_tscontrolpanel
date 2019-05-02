#pragma once

#include "common.h"
#include <tslib.h>

// === Defines ===

#define SLOTS 10
#define SEND_BUF_SIZE 128

// === Functions ===

// Sets up g_touch_device variable by assigning it to touch-device specified by dev_path
struct tsdev* open_tsdev(const char* dev_path);

// Create touch-signal sending socket
// Return sockfd if successful, -1 if failed
int create_tsconn(const char* addr, const char* port);

// Closes g_touch_device pointed device
int close_tsdev(struct tsdev* td);

// Closes g_sockfd pointed socket
int close_tsconn(int sockfd);

// Starts touch-signal forwarder, forwarding touch signals through socket file desgined by sockfd
int start_ts_forwarder(struct tsdev* td, int sockfd, uint32_t scrn_max_x, uint32_t scrn_max_y, uint32_t vm_x1, uint32_t vm_y1, uint32_t vm_x2, uint32_t vm_y2);

// Basic ts_lib testing
int test_tslib(struct tsdev* td);