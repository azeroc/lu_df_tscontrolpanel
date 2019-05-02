#include "common.h"
#include "server.h"
#include "vbox.h"

// Resource cleanup function (mostly for calling VBox cleanup)
static void cleanup(void)
{
	// VM cleanup
	free_vbox();
}

// Make sure to perform cleanup ops on signal events (such as interrupt or fatal os errors)
static void sigevent(int sig)
{
	printf("\n");
	// STDOUT/STDERR
	fflush(stderr);
	printf("Signal %d (%s) caught, beginning cleanup...\n", sig, sys_siglist[sig]);
	fflush(stdout);

	// Cleanup
	cleanup();

	exit(1);
}

int main(int argc, char* argv[])
{
	// Parse arguments and open touch-device with tslib
	if (argc < 2) {
		fprintf(stderr, "Usage: %s listenPort\n", argv[0]);
		return 1;
	}
	
	// Setup sig handlers
	signal(SIGSEGV, sigevent);
	signal(SIGINT, sigevent);
	signal(SIGTERM, sigevent);
	signal(SIGPIPE, sigevent); // Closed connection

	// Parse args
	uint16_t port = strtoul(argv[1], NULL, 10);

	// Initialize VBox objects
	if (init_vbox() != 0) {
		cleanup();
		return 1;
	}

	// Start the server
	if (start_tsserver(port) != 0)
	{
		cleanup();
		return 1;
	}

	cleanup();
	return 0;
}
