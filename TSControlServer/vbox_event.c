#include "vbox_event.h"

// Static state/thread variables

static atomic_bool vbox_event_stop = false;
static bool vbox_event_machine_shutdown = false;
static pthread_t vbox_event_thread;

// The VBox event handler
static HRESULT handleVBoxEvent(IEvent* event)
{
	VBoxEventType_T evType;
	HRESULT rc;

	if (!event)
	{
		printf("event null\n");
		return S_OK;
	}

	evType = VBoxEventType_Invalid;
	rc = IEvent_get_Type(event, &evType);
	if (FAILED(rc))
	{
		printf("cannot get event type, rc=%#x\n", rc);
		return S_OK;
	}

	switch (evType)
	{
	case VBoxEventType_OnGuestMultiTouch:
		printf("OnGuestMultiTouch\n");
		break;
	case VBoxEventType_OnMouseCapabilityChanged:
		printf("OnMouseCapabilityChanged:\n");
		BOOL supports_absolute = false;
		BOOL supports_relative = false;		
		BOOL supports_multitouch = false;
		IMouseCapabilityChangedEvent* ev = NULL;
		rc = IEvent_QueryInterface(event, &IID_IMouseCapabilityChangedEvent, (void**)& ev);
		if (FAILED(rc) || !ev)
		{
			printf("Cannot get IMouseCapabilityChangedEvent interface, rc=%#x\n", rc);
			return S_OK;
		}
		IMouseCapabilityChangedEvent_GetSupportsAbsolute(ev, &supports_absolute);
		IMouseCapabilityChangedEvent_GetSupportsRelative(ev, &supports_relative);
		IMouseCapabilityChangedEvent_GetSupportsMultiTouch(ev, &supports_multitouch);
		printf("\tIMouse support => ABS: %d, REL: %d, MT: %d\n", supports_absolute, supports_relative, supports_multitouch);
		IMouseCapabilityChangedEvent_Release(ev);
		break;
	case VBoxEventType_OnKeyboardLedsChanged:
		printf("OnKeyboardLedsChanged\n");
		break;
	case VBoxEventType_OnStateChanged:
	{
		IStateChangedEvent* ev = NULL;
		enum MachineState state;
		rc = IEvent_QueryInterface(event, &IID_IStateChangedEvent, (void**)& ev);
		if (FAILED(rc))
		{
			printf("cannot get StateChangedEvent interface, rc=%#x\n", rc);
			return S_OK;
		}
		if (!ev)
		{
			printf("StateChangedEvent reference null\n");
			return S_OK;
		}
		rc = IStateChangedEvent_get_State(ev, &state);
		if (FAILED(rc))
			printf("warning: cannot get state, rc=%#x\n", rc);
		IStateChangedEvent_Release(ev);
		printf("OnStateChanged: %s\n", GetStateName(state));
		fflush(stdout);
		if (state == MachineState_PoweredOff
			|| state == MachineState_Saved
			|| state == MachineState_Teleported
			|| state == MachineState_Aborted
			)
		{
			vbox_event_stop = true;
			vbox_event_machine_shutdown = true;
		}

		break;
	}
	case VBoxEventType_OnAdditionsStateChanged:
		printf("OnAdditionsStateChanged\n");
		break;
	case VBoxEventType_OnNetworkAdapterChanged:
		printf("OnNetworkAdapterChanged\n");
		break;
	case VBoxEventType_OnSerialPortChanged:
		printf("OnSerialPortChanged\n");
		break;
	case VBoxEventType_OnParallelPortChanged:
		printf("OnParallelPortChanged\n");
		break;
	case VBoxEventType_OnStorageControllerChanged:
		printf("OnStorageControllerChanged\n");
		break;
	case VBoxEventType_OnMediumChanged:
		printf("OnMediumChanged\n");
		break;
	case VBoxEventType_OnVRDEServerChanged:
		printf("OnVRDEServerChanged\n");
		break;
	case VBoxEventType_OnUSBControllerChanged:
		printf("OnUSBControllerChanged\n");
		break;
	case VBoxEventType_OnUSBDeviceStateChanged:
		printf("OnUSBDeviceStateChanged\n");
		break;
	case VBoxEventType_OnSharedFolderChanged:
		printf("OnSharedFolderChanged\n");
		break;
	case VBoxEventType_OnRuntimeError:
		printf("OnRuntimeError\n");
		break;
	case VBoxEventType_OnCanShowWindow:
		printf("OnCanShowWindow\n");
		break;
	case VBoxEventType_OnShowWindow:
		printf("OnShowWindow\n");
		break;
	default:
		printf("unknown event: %d\n", evType);
	}

	return S_OK;
}

// The pthread-offloaded vbox passive event handling
void* vbox_passive_events(void* argSession)
{
	ISession* session = argSession;
	bool failure = false;
	IConsole* console = NULL;
	HRESULT rc;

	rc = ISession_get_Console(session, &console);
	if (SUCCEEDED(rc) && console)
	{
		IEventSource* es = NULL;
		rc = IConsole_get_EventSource(console, &es);
		if (SUCCEEDED(rc) && es)
		{
			static const ULONG s_auInterestingEvents[] =
			{
				VBoxEventType_OnGuestMultiTouch,
				VBoxEventType_OnMousePointerShapeChanged,
				VBoxEventType_OnMouseCapabilityChanged,
				VBoxEventType_OnKeyboardLedsChanged,
				VBoxEventType_OnStateChanged,
				VBoxEventType_OnAdditionsStateChanged,
				VBoxEventType_OnNetworkAdapterChanged,
				VBoxEventType_OnSerialPortChanged,
				VBoxEventType_OnParallelPortChanged,
				VBoxEventType_OnStorageControllerChanged,
				VBoxEventType_OnMediumChanged,
				VBoxEventType_OnVRDEServerChanged,
				VBoxEventType_OnUSBControllerChanged,
				VBoxEventType_OnUSBDeviceStateChanged,
				VBoxEventType_OnSharedFolderChanged,
				VBoxEventType_OnRuntimeError,
				VBoxEventType_OnCanShowWindow,
				VBoxEventType_OnShowWindow
			};
			SAFEARRAY* interestingEventsSA = NULL;
			IEventListener* consoleListener = NULL;

			/* The VirtualBox API expects enum values as VT_I4, which in the
			 * future can be hopefully relaxed. */
			interestingEventsSA = g_pVBoxFuncs->pfnSafeArrayCreateVector(VT_I4, 0,
				sizeof(s_auInterestingEvents)
				/ sizeof(s_auInterestingEvents[0]));
			g_pVBoxFuncs->pfnSafeArrayCopyInParamHelper(interestingEventsSA, &s_auInterestingEvents,
				sizeof(s_auInterestingEvents));

			rc = IEventSource_CreateListener(es, &consoleListener);
			if (SUCCEEDED(rc) && consoleListener)
			{
				rc = IEventSource_RegisterListener(es, consoleListener, ComSafeArrayAsInParam(interestingEventsSA), 0 /* passive */);
				if (SUCCEEDED(rc))
				{

					fprintf(stdout, "Event handling loop started, PowerOff the machine or CTRL-C to terminate server\n");

					while (!vbox_event_stop)
					{
						IEvent* ev = NULL;
						rc = IEventSource_GetEvent(es, consoleListener, 250, &ev);
						if (FAILED(rc))
						{
							printf("Failed getting event: %#x\n", rc);
							vbox_event_stop = true;
							failure = true;
							continue;
						}
						/* handle timeouts, resulting in NULL events */
						if (!ev)
							continue;
						rc = handleVBoxEvent(ev);
						if (FAILED(rc))
						{
							printf("Failed processing event: %#x\n", rc);
							/* finish processing the event */
							vbox_event_stop = true;
							failure = true;
						}
						rc = IEventSource_EventProcessed(es, consoleListener, ev);
						if (FAILED(rc))
						{
							printf("Failed to mark event as processed: %#x\n", rc);
							/* continue with event release */
							vbox_event_stop = true;
							failure = true;
						}
						if (ev)
						{
							IEvent_Release(ev);
							ev = NULL;
						}
					}
				}
				else
				{
					fprintf(stderr, "Failed to register event listener\n");
					failure = true;
				}
				IEventSource_UnregisterListener(es, (IEventListener*)consoleListener);
				IEventListener_Release(consoleListener);
			}
			else
			{
				fprintf(stderr, "Failed to create an event listener instance\n");
				failure = true;
			}
			g_pVBoxFuncs->pfnSafeArrayDestroy(interestingEventsSA);
			IEventSource_Release(es);
		}
		else
		{
			fprintf(stderr, "Failed to get the event source instance\n");
			failure = true;
		}			
		IConsole_Release(console);
	}
	else
	{
		failure = true;
	}

	if (failure) // If event handling thread failed, raise interrupt signal to shut the entire process down
	{
		fprintf(stdout, "VBox event handling: failure encountered, raising interrupt to shutdown process...");
		raise(SIGINT);
	}
	if (vbox_event_machine_shutdown) // Shut down the server process because chosen VM is shut down
	{
		fprintf(stdout, "VBox event handling: VM is shutdown, raising interrupt to shutdown process...");
		raise(SIGINT);
	}

	return NULL;
}

// Initialize VBox passive events (starts a new event handler thread)
int init_vbox_events(ISession* session)
{
	fprintf(stdout, "Creating VBox event handling thread ...\n");
	if (pthread_create(&vbox_event_thread, NULL, vbox_passive_events, session) != 0)
	{
		perror("init_vbox_events - pthread_create");
		return EXIT_FAILURE;
	}
	return 0;
}

// Free/cleanup VBox passive events
int free_vbox_events(void)
{
	vbox_event_stop = true;
	pthread_join(vbox_event_thread, NULL);
	return 0;
}
