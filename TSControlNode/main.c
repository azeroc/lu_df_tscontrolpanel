#include "common.h"
#include "framebuffer.h" 
#include "touch_io.h"

// It's very important to properly "close" blank framebuffer, otherwise RPi's screen is unusable until system reboot
static void sigevent(int sig)
{
	close_framebuffer();
	fflush(stderr);
	printf("signal %d caught\n", sig);
	fflush(stdout);
	exit(1);
}

// Helper function to convert string to non-negative rectangle coords
// Returns 0 if coords are valid (i.e. x1 < x2, y1 < y2)
int parse_vmcoords(char* vm_x1_str, uint32_t* vm_x1, char* vm_y1_str, uint32_t* vm_y1, char* vm_x2_str, uint32_t* vm_x2, char* vm_y2_str, uint32_t* vm_y2)
{
	// Detect negatives
	if (vm_x1_str[0] == '-' || vm_y1_str[0] == '-' || vm_x2_str[0] == '-' || vm_y2_str[0] == '-') {
		return -1;
	}

	*vm_x1 = strtoul(vm_x1_str, NULL, 10);
	*vm_y1 = strtoul(vm_y1_str, NULL, 10);
	*vm_x2 = strtoul(vm_x2_str, NULL, 10);
	*vm_y2 = strtoul(vm_y2_str, NULL, 10);

	if (*vm_x1 >= *vm_x2) {
		return -1;
	}
	if (*vm_y1 >= *vm_y2) {
		return -1;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	uint32_t scrn_max_x;
	uint32_t scrn_max_y;
	uint32_t vm_x1, vm_y1, vm_x2, vm_y2;
	int ecode;
	struct tsdev* td;
	int sockfd;

	// Check if args are present
	if (argc < 10) {
		fprintf(stderr, "Usage: %s /dev/input/eventX serveraddr serverport screen_max_x screen_max_y vm_x1 vm_y1 vm_x2 vm_y2\n", argv[0]);
		return 1;
	}

	// Parse screen_max_x and screen_max_y
	if (argv[4][0] == '-' || argv[5][0] == '-') {
		fprintf(stderr, "This touch device's screen_max_x screen_max_y must be positive numbers\n");
		return 1;
	}
	scrn_max_x = strtoul(argv[4], NULL, 10);
	scrn_max_y = strtoul(argv[5], NULL, 10);

	// Parse VM boundary arguments
	if (parse_vmcoords(argv[6], &vm_x1, argv[7], &vm_y1, argv[8], &vm_x2, argv[9], &vm_y2) != 0) {
		fprintf(stderr, "VM Screen boundary rects vm_x1, vm_y1, vm_x2, vm_y2 must be positive and valid coordinates\n");
		return 1;
	}

	// Setup touch device
	td = open_tsdev(argv[1]);
	if (td == NULL) {
		return 1;
	}

	// Setup socket touch-signal forwarding TCP/IP connection
	sockfd = create_tsconn(argv[2], argv[3]);
	if (sockfd < 0) {
		close_tsdev(td);
		return 1;
	}

	// Setup sig handlers
	signal(SIGSEGV, sigevent);
	signal(SIGINT, sigevent);
	signal(SIGTERM, sigevent);
	signal(SIGPIPE, sigevent); // Closed connection

	// Make screen blank (to avoid touch-clicking rpi desktop components)
	open_framebuffer();
	ecode = start_ts_forwarder(td, sockfd, scrn_max_x, scrn_max_y, vm_x1, vm_y1, vm_x2, vm_y2);
	if (ecode != 0) {
		close_tsdev(td);
		close_tsconn(sockfd);
	}
	close_framebuffer();

	return ecode;
}
