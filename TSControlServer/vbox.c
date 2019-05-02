#include "vbox.h"
#include "vbox_helper.h"
#include "vbox_event.h"
#include "vbox_mouse.h"

// Internal static variables for VBox objects

static IVirtualBoxClient* s_vboxclient = NULL;
static IVirtualBox*       s_vbox = NULL;
static ISession*          s_vboxsession = NULL;
static IMachine*          s_vboxmachine = NULL;

// === TSControlServer VBox API access layer methods ===

// Initialize VBox objects
int init_vbox(void)
{
	ULONG   revision = 0;
	BSTR    versionUtf16 = NULL;
	BSTR    homefolderUtf16 = NULL;
	HRESULT rc;

	fprintf(stdout, "Starting to initialize VBox objects ...\n");

	// Initialize COM/XPCOM GLUE layer
	if (VBoxCGlueInit())
	{
		fprintf(stderr, "FATAL: VBoxCGlueInit failed: %s\n", g_szVBoxErrMsg);
		return EXIT_FAILURE;
	}

	// Print VBox API versions
	{
		unsigned int ver = g_pVBoxFuncs->pfnGetVersion();
		fprintf(stdout, "VirtualBox version: %u.%u.%u\n", ver / 1000000, ver / 1000 % 1000, ver % 1000);
		ver = g_pVBoxFuncs->pfnGetAPIVersion();
		fprintf(stdout, "VirtualBox API version: %u.%u\n", ver / 1000, ver % 1000);
	}

	// Initialize VBoxClient
	g_pVBoxFuncs->pfnClientInitialize(NULL, &s_vboxclient);
	if (!s_vboxclient)
	{
		fprintf(stderr, "FATAL: could not get VirtualBoxClient reference\n");
		return EXIT_FAILURE;
	}

	// Acquire VBox object from VBox client
	rc = IVirtualBoxClient_get_VirtualBox(s_vboxclient, &s_vbox);
	if (FAILED(rc) || !s_vbox)
	{
		PrintErrorInfo("FATAL: could not get VirtualBox reference", rc);
		return EXIT_FAILURE;
	}

	// Acquire VBox session object from VBox client
	rc = IVirtualBoxClient_get_Session(s_vboxclient, &s_vboxsession);
	if (FAILED(rc) || !s_vboxsession)
	{
		PrintErrorInfo("FATAL: could not get Session reference", rc);
		return EXIT_FAILURE;
	}

	printf("----------------------------------------------------\n");

	// VBox object revision
	rc = IVirtualBox_get_Revision(s_vbox, &revision);
	if (SUCCEEDED(rc))
	{
		printf("\tRevision: %u\n", revision);
	}
	else
	{
		PrintErrorInfo("GetRevision() failed", rc);
	}

	// VBox object version
	rc = IVirtualBox_get_Version(s_vbox, &versionUtf16);
	if (SUCCEEDED(rc))
	{
		char* version = NULL;
		g_pVBoxFuncs->pfnUtf16ToUtf8(versionUtf16, &version);
		printf("\tVersion: %s\n", version);
		g_pVBoxFuncs->pfnUtf8Free(version);
		g_pVBoxFuncs->pfnComUnallocString(versionUtf16);
	}
	else
	{
		PrintErrorInfo("GetVersion() failed", rc);
	}		

	// VBox object home directory (typically ~/.config/VirtualBox)
	rc = IVirtualBox_get_HomeFolder(s_vbox, &homefolderUtf16);
	if (SUCCEEDED(rc))
	{
		char* homefolder = NULL;
		g_pVBoxFuncs->pfnUtf16ToUtf8(homefolderUtf16, &homefolder);
		printf("\tVBoxHome: %s\n", homefolder);
		g_pVBoxFuncs->pfnUtf8Free(homefolder);
		g_pVBoxFuncs->pfnComUnallocString(homefolderUtf16);
	}
	else
	{
		PrintErrorInfo("GetHomeFolder() failed", rc);
	}

	// List VMs and let user pick one of them to start
	printf("----------------------------------------------------\n");
	if (listVMs(s_vbox, s_vboxsession, &s_vboxmachine) != 0) 
	{
		return EXIT_FAILURE;
	}

	// Register and start event handling (event loop is on a different thread)
	if (init_vbox_events(s_vboxsession) != 0)
	{
		return EXIT_FAILURE;
	}

	// Create IMouse interface objects
	if (init_vbox_mouse(s_vboxsession) != 0)
	{
		return EXIT_FAILURE;
	}

	fprintf(stdout, "VBox initialization successful!\n");
	return 0;
}

// Destroy/free VBox objects
void free_vbox(void)
{
	// Cleanup IMouse interface objects
	fprintf(stdout, "VBox cleanup: VBox IMouse object cleanup ...\n");
	free_vbox_mouse();

	// Stop and cleanup event handling
	fprintf(stdout, "VBox cleanup: VBox event handling cleanup ...\n");
	free_vbox_events();

	// Poweroff VM
	fprintf(stdout, "VBox cleanup: VBox VM shutdown ...\n");
	stopVM(s_vboxsession);

	// Unlock session
	fprintf(stdout, "VBox cleanup: Unlocking VBox session ...\n");
	ISession_UnlockMachine(s_vboxsession);

	// Release VBox objects
	fprintf(stdout, "VBox cleanup: Releasing VBox objects ...\n");
	if (s_vboxsession)
	{
		ISession_Release(s_vboxsession);
		s_vboxsession = NULL;
	}
	if (s_vbox)
	{
		IVirtualBox_Release(s_vbox);
		s_vbox = NULL;
	}
	if (s_vboxclient)
	{
		IVirtualBoxClient_Release(s_vboxclient);
		s_vboxclient = NULL;
	}
	if (s_vboxmachine)
	{
		IMachine_Release(s_vboxmachine);
		s_vboxmachine = NULL;
	}

	// C GLUE binding cleanup
	fprintf(stdout, "VBox cleanup: C GLUE binding cleanup ...\n");
	g_pVBoxFuncs->pfnClientUninitialize();
	VBoxCGlueTerm();

	fprintf(stdout, "VBox cleanup: Done\n");
}
