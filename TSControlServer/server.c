#include "server.h"

// Starts VBox touch signal server, uses select polling on sockets instead of usual new-connection-new-thread pattern
int start_tsserver(uint16_t listenPort)
{
	int i; // Sockfd iteration
	int j; // Read buf iteration
	int nbytes; // Read bytes
	struct touch_sig tsig;	
	char ipv4_str[INET_ADDRSTRLEN];	
	char read_buf[READBUF_SIZE];
	char* rbend; // Read buf end pointer
	char* rbptr; // Read buf pointer

	// Setup for select() polling, init/clear fd_sets
	fd_set master;
	fd_set read_fds;
	int fdmax;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
	// Socket & addr vars
	int addrlen; // For clientaddr len ref
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	int listensockfd;
	int newfd;

	// Listen socket init
	int reuseaddr = 1;
	listensockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listensockfd < 0) {
		perror("init: socket");
		return 1;
	}
	if (setsockopt(listensockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1)
	{
		perror("init: setsockopt");
		return 1;
	}

	// Server addr binding
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(listenPort);
	memset(&(serveraddr.sin_zero), '\0', 8);
	if (bind(listensockfd, (struct sockaddr*) & serveraddr, sizeof(serveraddr)) == -1)
	{
		perror("init: bind");
		return 1;
	}

	// Server listening (can accept up to MAXCONNS)
	if (listen(listensockfd, MAXCONNS) == -1)
	{
		perror("init: listen");
		return 1;
	}

	// Add listening socket to master fd_set, set it as fd maximum
	FD_SET(listensockfd, &master);
	fdmax = listensockfd;

	// Report success of setup
	fprintf(stdout, "Server listening on port %d ...\n", listenPort);

	// The accept, select, touch parsing loop
	for (;;) 
	{
		read_fds = master;

		// Select from read sets
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("select");
			return 1;
		}

		// Check for data to be read
		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &read_fds))
			{
				// Accept and fd_set master register new connections
				if (i == listensockfd)
				{
					addrlen = sizeof(clientaddr);
					if ((newfd = accept(listensockfd, (struct sockaddr*) & clientaddr, &addrlen)) == -1)
					{
						perror("accept");
					}
					else 
					{
						FD_SET(newfd, &master);
						if (newfd > fdmax)
						{
							fdmax = newfd;
						}

						inet_ntop(AF_INET, &(clientaddr.sin_addr), ipv4_str, INET_ADDRSTRLEN);
						fprintf(stdout, "Accepted connection: %s:%u\n", ipv4_str, clientaddr.sin_port);
					}
				}
				else // ... else one of the active connections have data to be read
				{
					// Read sock_stream data bytes
					if ((nbytes = recv(i, read_buf, READBUF_SIZE-1, 0)) <= 0)
					{
						getpeername(i, (struct sockaddr*) & clientaddr, &addrlen);
						inet_ntop(AF_INET, &(clientaddr.sin_addr), ipv4_str, INET_ADDRSTRLEN);
						if (nbytes == 0) // Closed connection
						{
							fprintf(stdout, "Closed connection: %s:%u\n", ipv4_str, clientaddr.sin_port);
						}
						else // Some weird recv error
						{
							perror("client recv");
						}
						close(i); // Either way, close it
						FD_CLR(i, &master); // Also make sure to remove it from master list (otherwise select will return Bad file descriptor)
					}
					else
					{
						// Set end of read buf, then null-terminate it to be safe for string functions
						rbend = read_buf + nbytes;
						*rbend = '\0';

						// Parse touch_sig structs from received input buf until reaching end of read buffer
						rbptr = read_buf;
						while (rbptr < rbend)
						{
							if (deserialize_touch_sig(&tsig, read_buf) == -1)
							{
								fprintf(stderr, "Received bad touch_sig data, raw string: %s\n", read_buf);
							}
							else
							{
								// Print received input if DEBUG
								fprintf(stdout, "Received touch sig (X Y SLOT TRACK_ID STATE): " TOUCH_SIG_STRFMT, tsig.x, tsig.y, tsig.slot, tsig.tracking_id, tsig.touch_state);

								// Emulate touch signal on VBox
								vbox_handle_touchsig(tsig);
							}

							// Find newline terminator '\n' which separates touch_sig struct data
							rbptr = strchr(rbptr, '\n');
							if (rbptr == NULL)
							{
								break;
							}
							else
							{
								rbptr++; // Skip '\n'
								rbptr++; // Skip following '\0'
								if (*rbptr == '\0') { break; } // In case we reach '\0' set at the end of read input
							}
						}
					}
				}
			}
		}
	}
	return 0;
}