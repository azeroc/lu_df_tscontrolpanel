#include "vbox_mouse.h"

// Static IMouse variables
static ISession* s_vboxmousesession = NULL;
static IMouse* s_vboxmouse = NULL;
static IConsole* s_vboxmouseconsole = NULL;

// Initialize VBox IMouse interface objects to allow mouse/touch input actions
int init_vbox_mouse(ISession* session)
{
	s_vboxmousesession = session;
	fprintf(stdout, "Preparing VBox mouse/touch control setup ...\n");
	HRESULT rc;
	rc = ISession_get_Console(s_vboxmousesession, &s_vboxmouseconsole);
	if (SUCCEEDED(rc) && s_vboxmouseconsole)
	{
		rc = IConsole_get_Mouse(s_vboxmouseconsole, &s_vboxmouse);
		if (SUCCEEDED(rc) && s_vboxmouse)
		{
			fprintf(stdout, "VBox IMouse object created\n");
		}
		else
		{
			PrintErrorInfo("FATAL: Failed to get IMouse for VBox mouse/touch control setup", rc);
			return EXIT_FAILURE;
		}
	}
	else
	{
		PrintErrorInfo("FATAL: Failed to get IConsole for VBox mouse/touch control setup", rc);
		return EXIT_FAILURE;
	}
	return 0;
}

// Handle touch_sig structure and emulate touch signal in VM
int vbox_handle_touchsig(struct touch_sig tsig)
{
	// Var setup
	HRESULT rc;
	LONG count = 1;
	ULONG contactSize = 1;
	LONG64 encoded;
	LONG64* contacts;
	ULONG scanTime = 0;

	// Get millisecond time
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		scanTime = round(ts.tv_nsec / 1.0e6); // Nanoseconds -> Milliseconds
	}

	// Encode x,y,id,states into 64bit integer
	int16_t x = (int16_t)tsig.x; // Restrict to 16 bits
	int16_t y = (int16_t)tsig.y; // Restrict to 16 bits
	uint8_t id = (uint8_t)tsig.slot; // Restrict to 8 bits
	uint8_t special; // Restrict to 8 bits
	if (tsig.touch_state == 0)
	{
		special = 0;
	}
	else
	{
		special = 0b11;
	}
	encoded =  (LONG64)x;               // X: 0 byte offset
	encoded += ((LONG64)y)  << 16;      // Y: 2 byte offset
	encoded += ((LONG64)id) << 32;      // ID: 4 byte offset
	encoded += ((LONG64)special) << 40; // Special: 5 byte offset
	uint64_t a;
	// Copy encoded result to contacts single element array
	contacts = malloc(sizeof(LONG64));
	contacts[0] = encoded;

	// Send the multi-touch action
	fprintf(stdout, "IMouse: sending multi-touch event data: %ld\n", contacts[0]);

	// VBox API bug - IMouse_PutEventMultiTouch macro function requires 4 args, but points to 5 arg function
	// Workaround - skip API convenience macro
	rc = s_vboxmouse->lpVtbl->PutEventMultiTouch(s_vboxmouse, count, contactSize, contacts, scanTime);
	if (!SUCCEEDED(rc))
	{
		PrintErrorInfo("IMouse: IMouse_PutEventMultiTouch failure", rc);
	}

	// Cleanup
	free(contacts);
	return 0;
}

// Cleanup VBox IMouse interface objects
int free_vbox_mouse(void)
{
	if (s_vboxmouse)
	{
		IMouse_Release(s_vboxmouse);
	}
	if (s_vboxmouseconsole)
	{
		IConsole_Release(s_vboxmouseconsole);
	}	
	return 0;
}
