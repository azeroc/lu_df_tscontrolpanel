#include "touch_io.h"
#include "touch_sig.h"

// === Functions ===

// Sets up g_touch_device variable by assigning it to touch-device specified by dev_path
struct tsdev* open_tsdev(const char* dev_path)
{
	return ts_setup(dev_path, 0);
}

// Create touch-signal sending socket
// Return sockfd if successful, -1 if failed
int create_tsconn(const char* addr, const char* port)
{
	int sockfd = -1;
	struct sockaddr_in server_addr;

	// Convert to uint16 port
	uint16_t uintport = strtoul(port, NULL, 10);

	// Set sockaddr properties
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(uintport);
	server_addr.sin_addr.s_addr = inet_addr(addr);

	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		perror("socket creation");
		return -1;
	}

	// Connect the socket
	int bindCode = connect(sockfd, (struct sockaddr*) & server_addr, sizeof server_addr);
	if (bindCode < 0) 
	{
		perror("connect");
		return -1;
	}

	return sockfd;
}

// Closes g_touch_device pointed device
int close_tsdev(struct tsdev* td)
{
	return ts_close(td);
}

// Closes g_sockfd pointed socket
int close_tsconn(int sockfd) 
{
	return close(sockfd);
}

// Calculate VM coord from given coord, its max val and VM boundaries
uint32_t calc_vmcoord(uint32_t coord, uint32_t coord_max, uint32_t vmc1, uint32_t vmc2) 
{
	// Force coord to be within coord_max bounds
	if (coord > coord_max) { coord = coord_max; }

	// Scale coord from one coord system to another
	uint32_t vm_coord_diff = vmc2 - vmc1;
	float mod = 1.0 * vm_coord_diff / coord_max;
	float res_scaled = coord * mod;
	uint32_t res = vmc1 + res_scaled; // vm-coord-1 offset + scaled result coord
	return res;
}

// Starts touch-signal forwarder, forwarding touch signals through socket file desgined by g_sockfd and g_touch_device
int start_ts_forwarder(struct tsdev* td, int sockfd, uint32_t scrn_max_x, uint32_t scrn_max_y, uint32_t vm_x1, uint32_t vm_y1, uint32_t vm_x2, uint32_t vm_y2)
{
	// Setup (memory memory allocation)
	int i, j, e;
	struct touch_sig tsig;
	char* sendbuf = malloc(SEND_BUF_SIZE);
	struct ts_sample_mt** sample_mt_arr = malloc(sizeof(struct ts_sample_mt*));
	if (!sample_mt_arr)
	{
		fprintf(stderr, "sample_mt_arr malloc err");
		return -ENOMEM;
	}
	sample_mt_arr[0] = calloc(SLOTS, sizeof(struct ts_sample_mt));
	if (!sample_mt_arr[0]) {
		free(sample_mt_arr);
		fprintf(stderr, "sample_mt_arr[i] calloc err");
		return -ENOMEM;
	}

	// Endless read-loop
	for (;;) 
	{
		// Read sample
		int samples_read = ts_read_mt(td, sample_mt_arr, SLOTS, 1);
		if (samples_read < 0) {
			fprintf(stderr, "ts_read_mt err");
			return -1;
		}

		// Loop through different mt slot samples
		for (i = 0; i < SLOTS; i++) {
			if (sample_mt_arr[0][i].valid) {
				// Fix negative tslib coords due to bad calibration
				int ts_x = sample_mt_arr[0][i].x;
				if (ts_x < 0) { ts_x = 0; }
				int ts_y = sample_mt_arr[0][i].y;
				if (ts_y < 0) { ts_y = 0; }

				// Copy values to touch_sig struct
				tsig.x = ts_x;
				tsig.y = ts_y;
				tsig.slot = sample_mt_arr[0][i].slot;
				tsig.tracking_id = sample_mt_arr[0][i].tracking_id;
				tsig.touch_state = sample_mt_arr[0][i].pen_down;

				// Convert touch coords to chosen rect VM coords
				tsig.x = calc_vmcoord(tsig.x, scrn_max_x, vm_x1, vm_x2);
				tsig.y = calc_vmcoord(tsig.y, scrn_max_y, vm_y1, vm_y2);

				// Serialize touch_sig
				e = serialize_touch_sig(tsig, sendbuf, SEND_BUF_SIZE);
				if (e != 0) {
					fprintf(stderr, "serialize_touch_sig failure, errcode: %d", e);
					return -1;
				}
				fprintf(stdout, "Serialized tsig: %s", sendbuf);

				// Send serialized touch_sig buf
				e = send(sockfd, sendbuf, strlen(sendbuf), 0);
				if (e < 0) {
					perror("start_ts_forwarder->send");
					return -1;
				}
			}
		}
	}

	return 0;
}

// Basic ts_lib testing
int test_tslib(struct tsdev* td)
{
	int i;
	struct ts_sample_mt** sample_mt_arr = malloc(sizeof(struct ts_sample_mt*));
	if (!sample_mt_arr) 
	{
		fprintf(stderr, "sample_mt_arr malloc err");
		return -ENOMEM;
	}
	sample_mt_arr[0] = calloc(SLOTS, sizeof(struct ts_sample_mt));
	if (!sample_mt_arr[0]) 
	{
		free(sample_mt_arr);
		ts_close(td);
		fprintf(stderr, "sample_mt_arr[i] calloc err");
		return -ENOMEM;
	}
	while (1)
	{
		int samples_read = ts_read_mt(td, sample_mt_arr, SLOTS, 1);
		printf("samples_read = %d\n", samples_read);
		if (samples_read < 0) {
			fprintf(stderr, "ts_read_mt err");
			return -1;
		}
		for (i = 0; i < SLOTS; i++) {
			if (sample_mt_arr[0][i].valid) {
				printf("SAMPLE[0][%d]: X = %d, Y = %d, SLOT = %d, TRACKID = %d, PEN_DOWN = %d\n",
					i,
					sample_mt_arr[0][i].x,
					sample_mt_arr[0][i].y,
					sample_mt_arr[0][i].slot,
					sample_mt_arr[0][i].tracking_id,
					sample_mt_arr[0][i].pen_down);
			}
		}
		printf("\n");
	}
}