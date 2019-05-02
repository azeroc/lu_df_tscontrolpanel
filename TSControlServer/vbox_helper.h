#pragma once
#include "common.h"
#include "VBoxCAPIGlue.h"

// === VBox helper methods (taken from tstCAPIGlue.c sample) ===

// Get VBox VM machine state string
static const char* GetStateName(MachineState_T machineState)
{
	switch (machineState)
	{
	case MachineState_Null:                   return "<null>";
	case MachineState_PoweredOff:             return "PoweredOff";
	case MachineState_Saved:                  return "Saved";
	case MachineState_Teleported:             return "Teleported";
	case MachineState_Aborted:                return "Aborted";
	case MachineState_Running:                return "Running";
	case MachineState_Paused:                 return "Paused";
	case MachineState_Stuck:                  return "Stuck";
	case MachineState_Teleporting:            return "Teleporting";
	case MachineState_LiveSnapshotting:       return "LiveSnapshotting";
	case MachineState_Starting:               return "Starting";
	case MachineState_Stopping:               return "Stopping";
	case MachineState_Saving:                 return "Saving";
	case MachineState_Restoring:              return "Restoring";
	case MachineState_TeleportingPausedVM:    return "TeleportingPausedVM";
	case MachineState_TeleportingIn:          return "TeleportingIn";
	case MachineState_FaultTolerantSyncing:   return "FaultTolerantSyncing";
	case MachineState_DeletingSnapshotOnline: return "DeletingSnapshotOnline";
	case MachineState_DeletingSnapshotPaused: return "DeletingSnapshotPaused";
	case MachineState_RestoringSnapshot:      return "RestoringSnapshot";
	case MachineState_DeletingSnapshot:       return "DeletingSnapshot";
	case MachineState_SettingUp:              return "SettingUp";
	default:                                  return "no idea";
	}
}

/**
 * Print detailed error information if available.
 * @param   pszErrorMsg     string containing the code location specific error message
 * @param   rc              COM/XPCOM result code
 */
static void PrintErrorInfo(const char* pszErrorMsg, HRESULT rc)
{
	IErrorInfo* ex;
	HRESULT rc2;
	fprintf(stderr, "%s (rc=%#010x)\n", pszErrorMsg, (unsigned)rc);

	rc2 = g_pVBoxFuncs->pfnGetException(&ex);
	if (SUCCEEDED(rc2) && ex)
	{
		IVirtualBoxErrorInfo* ei;
		rc2 = IErrorInfo_QueryInterface(ex, &IID_IVirtualBoxErrorInfo, (void**)& ei);
		if (SUCCEEDED(rc2) && ei != NULL)
		{
			/* got extended error info, maybe multiple infos */
			do
			{
				LONG resultCode = S_OK;
				BSTR componentUtf16 = NULL;
				char* component = NULL;
				BSTR textUtf16 = NULL;
				char* text = NULL;
				IVirtualBoxErrorInfo* ei_next = NULL;
				fprintf(stderr, "Extended error info (IVirtualBoxErrorInfo):\n");

				IVirtualBoxErrorInfo_get_ResultCode(ei, &resultCode);
				fprintf(stderr, "  resultCode=%#010x\n", (unsigned)resultCode);

				IVirtualBoxErrorInfo_get_Component(ei, &componentUtf16);
				g_pVBoxFuncs->pfnUtf16ToUtf8(componentUtf16, &component);
				g_pVBoxFuncs->pfnComUnallocString(componentUtf16);
				fprintf(stderr, "  component=%s\n", component);
				g_pVBoxFuncs->pfnUtf8Free(component);

				IVirtualBoxErrorInfo_get_Text(ei, &textUtf16);
				g_pVBoxFuncs->pfnUtf16ToUtf8(textUtf16, &text);
				g_pVBoxFuncs->pfnComUnallocString(textUtf16);
				fprintf(stderr, "  text=%s\n", text);
				g_pVBoxFuncs->pfnUtf8Free(text);

				rc2 = IVirtualBoxErrorInfo_get_Next(ei, &ei_next);
				if (FAILED(rc2))
					ei_next = NULL;
				IVirtualBoxErrorInfo_Release(ei);
				ei = ei_next;
			} while (ei);
		}

		IErrorInfo_Release(ex);
		g_pVBoxFuncs->pfnClearException();
	}
}

// Shut down VirtualBox machine
static void stopVM(ISession* session)
{
	IConsole* console = NULL;
	IProgress* progress = NULL;
	HRESULT rc;

	rc = ISession_get_Console(session, &console);
	if (SUCCEEDED(rc) && console)
	{
		rc = IConsole_PowerDown(console, &progress);
		if (SUCCEEDED(rc))
		{
			BOOL completed;
			LONG resultCode;

			printf("Waiting for VM to shut down...\n");
			IProgress_WaitForCompletion(progress, -1);

			rc = IProgress_get_Completed(progress, &completed);
			if (FAILED(rc))
				fprintf(stderr, "Error: GetCompleted status failed\n");

			IProgress_get_ResultCode(progress, &resultCode);
			if (FAILED(resultCode))
			{
				IVirtualBoxErrorInfo* errorInfo;
				BSTR textUtf16;
				char* text;

				IProgress_get_ErrorInfo(progress, &errorInfo);
				IVirtualBoxErrorInfo_get_Text(errorInfo, &textUtf16);
				g_pVBoxFuncs->pfnUtf16ToUtf8(textUtf16, &text);
				printf("Error: %s\n", text);

				g_pVBoxFuncs->pfnComUnallocString(textUtf16);
				g_pVBoxFuncs->pfnUtf8Free(text);
				IVirtualBoxErrorInfo_Release(errorInfo);
			}
			else
			{
				fprintf(stdout, "VM process has been successfully shut down\n");
			}
			IProgress_Release(progress);
		}
		IConsole_Release(console);
	}
	else
	{
		fprintf(stdout, "VM likely already shut down\n");
	}
}

/**
 * Start a VM.
 *
 * @param   vbox  ptr to IVirtualBox object
 * @param   session     ptr to ISession object
 * @praam   machine     ptr of ptr to IMachine object
 * @param   id          identifies the machine to start
 *
 * @return              Return code (0 - success, 1 - failure)
 */
static int startVM(IVirtualBox* vbox, ISession* session, IMachine** machine, BSTR id)
{
	HRESULT rc;
	IProgress* progress = NULL;
	BSTR env = NULL;
	BSTR sessionType;
	SAFEARRAY* groupsSA = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();

	rc = IVirtualBox_FindMachine(vbox, id, machine);
	if (FAILED(rc) || !*machine)
	{
		PrintErrorInfo("Error: Couldn't get the Machine reference", rc);
		return EXIT_FAILURE;
	}

	rc = IMachine_get_Groups(*machine, ComSafeArrayAsOutTypeParam(groupsSA, BSTR));
	if (SUCCEEDED(rc))
	{
		BSTR* groups = NULL;
		ULONG cbGroups = 0;
		ULONG i, cGroups;
		g_pVBoxFuncs->pfnSafeArrayCopyOutParamHelper((void**)& groups, &cbGroups, VT_BSTR, groupsSA);
		g_pVBoxFuncs->pfnSafeArrayDestroy(groupsSA);
		cGroups = cbGroups / sizeof(groups[0]);
		for (i = 0; i < cGroups; ++i)
		{
			/* Note that the use of %S might be tempting, but it is not
			 * available on all platforms, and even where it is usable it
			 * may depend on correct compiler options to make wchar_t a
			 * 16 bit number. So better play safe and use UTF-8. */
			char* group;
			g_pVBoxFuncs->pfnUtf16ToUtf8(groups[i], &group);
			printf("Groups[%d]: %s\n", i, group);
			g_pVBoxFuncs->pfnUtf8Free(group);
		}
		for (i = 0; i < cGroups; ++i)
			g_pVBoxFuncs->pfnComUnallocString(groups[i]);
		g_pVBoxFuncs->pfnArrayOutFree(groups);
	}

	g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &sessionType);
	rc = IMachine_LaunchVMProcess(*machine, session, sessionType, env, &progress);
	g_pVBoxFuncs->pfnUtf16Free(sessionType);
	if (SUCCEEDED(rc))
	{
		BOOL completed;
		LONG resultCode;

		printf("Waiting for the remote session to open...\n");
		IProgress_WaitForCompletion(progress, -1);

		rc = IProgress_get_Completed(progress, &completed);
		if (FAILED(rc))
			fprintf(stderr, "Error: GetCompleted status failed\n");

		IProgress_get_ResultCode(progress, &resultCode);
		if (FAILED(resultCode))
		{
			IVirtualBoxErrorInfo* errorInfo;
			BSTR textUtf16;
			char* text;

			IProgress_get_ErrorInfo(progress, &errorInfo);
			IVirtualBoxErrorInfo_get_Text(errorInfo, &textUtf16);
			g_pVBoxFuncs->pfnUtf16ToUtf8(textUtf16, &text);
			printf("Error: %s\n", text);

			g_pVBoxFuncs->pfnComUnallocString(textUtf16);
			g_pVBoxFuncs->pfnUtf8Free(text);
			IVirtualBoxErrorInfo_Release(errorInfo);
		}
		else
		{
			fprintf(stderr, "VM process has been successfully started\n");
		}
		IProgress_Release(progress);
	}
	else
	{
		PrintErrorInfo("Error: LaunchVMProcess failed", rc);
		return 1;
	}

	return 0;
}

/**
 * List the registered VMs and offer option to start one of them
 *
 * @param   vbox  ptr to IVirtualBox object
 * @param   session     ptr to ISession object
 * @praam   runMachine  ptr of ptr to IMachine object (to be set to point to ptr of launched VM)
 */
static int listVMs(IVirtualBox* vbox, ISession* session, IMachine** runMachine)
{
	HRESULT rc;
	SAFEARRAY* machinesSA = g_pVBoxFuncs->pfnSafeArrayOutParamAlloc();
	IMachine** machines = NULL;
	ULONG machineCnt = 0;
	ULONG i;
	unsigned start_id;

	/*
	 * Get the list of all registered VMs.
	 */
	rc = IVirtualBox_get_Machines(vbox, ComSafeArrayAsOutIfaceParam(machinesSA, IMachine*));
	if (FAILED(rc))
	{
		PrintErrorInfo("could not get list of machines", rc);
		return 1;
	}

	/*
	 * Extract interface pointers from machinesSA, and update the reference
	 * counter of each object, as destroying machinesSA would call Release.
	 */
	g_pVBoxFuncs->pfnSafeArrayCopyOutIfaceParamHelper((IUnknown * **)& machines, &machineCnt, machinesSA);
	g_pVBoxFuncs->pfnSafeArrayDestroy(machinesSA);

	if (!machineCnt)
	{
		g_pVBoxFuncs->pfnArrayOutFree(machines);
		printf("\tNo VMs\n");
		return 1;
	}

	printf("VM List:\n\n");

	/*
	 * Iterate through the collection.
	 */
	for (i = 0; i < machineCnt; ++i)
	{
		IMachine* machine = machines[i];
		BOOL      isAccessible = FALSE;

		printf("\tMachine #%u\n", (unsigned)i);

		if (!machine)
		{
			printf("\t(skipped, NULL)\n");
			continue;
		}

		IMachine_get_Accessible(machine, &isAccessible);

		if (isAccessible)
		{
			BSTR machineNameUtf16;
			char* machineName;

			IMachine_get_Name(machine, &machineNameUtf16);
			g_pVBoxFuncs->pfnUtf16ToUtf8(machineNameUtf16, &machineName);
			g_pVBoxFuncs->pfnComUnallocString(machineNameUtf16);
			printf("\tName:        %s\n", machineName);
			g_pVBoxFuncs->pfnUtf8Free(machineName);
		}
		else
			printf("\tName:        <inaccessible>\n");

		{
			BSTR uuidUtf16;
			char* uuidUtf8;

			IMachine_get_Id(machine, &uuidUtf16);
			g_pVBoxFuncs->pfnUtf16ToUtf8(uuidUtf16, &uuidUtf8);
			g_pVBoxFuncs->pfnComUnallocString(uuidUtf16);
			printf("\tUUID:        %s\n", uuidUtf8);
			g_pVBoxFuncs->pfnUtf8Free(uuidUtf8);
		}

		if (isAccessible)
		{
			{
				BSTR  configFileUtf16;
				char* configFileUtf8;

				IMachine_get_SettingsFilePath(machine, &configFileUtf16);
				g_pVBoxFuncs->pfnUtf16ToUtf8(configFileUtf16, &configFileUtf8);
				g_pVBoxFuncs->pfnComUnallocString(configFileUtf16);
				printf("\tConfig file: %s\n", configFileUtf8);
				g_pVBoxFuncs->pfnUtf8Free(configFileUtf8);
			}

			{
				ULONG memorySize;

				IMachine_get_MemorySize(machine, &memorySize);
				printf("\tMemory size: %uMB\n", memorySize);
			}

			{
				ULONG vramSize;

				IMachine_get_VRAMSize(machine, &vramSize);
				printf("\tVRAM size:   %uMB\n", vramSize);
			}

			{
				BSTR typeId;
				BSTR osNameUtf16;
				char* osName;
				IGuestOSType* osType = NULL;

				IMachine_get_OSTypeId(machine, &typeId);
				IVirtualBox_GetGuestOSType(vbox, typeId, &osType);
				g_pVBoxFuncs->pfnComUnallocString(typeId);
				IGuestOSType_get_Description(osType, &osNameUtf16);
				g_pVBoxFuncs->pfnUtf16ToUtf8(osNameUtf16, &osName);
				g_pVBoxFuncs->pfnComUnallocString(osNameUtf16);
				printf("\tGuest OS:    %s\n\n", osName);
				g_pVBoxFuncs->pfnUtf8Free(osName);

				IGuestOSType_Release(osType);
			}
		}
	}

	/*
	 * Let the user chose a machine to start.
	 */
	printf("Type Machine# to start (0 - %u) or 'quit' to exit: ",
		(unsigned)(machineCnt - 1));
	fflush(stdout);

	if (scanf("%u", &start_id) == 1 && start_id < machineCnt)
	{
		IMachine* machine = machines[start_id];

		if (machine)
		{
			BSTR uuidUtf16 = NULL;

			IMachine_get_Id(machine, &uuidUtf16);
			if (startVM(vbox, session, runMachine, uuidUtf16) != 0)
			{
				return EXIT_FAILURE;
			}
			g_pVBoxFuncs->pfnComUnallocString(uuidUtf16);
		}
		else
		{
			return EXIT_FAILURE;
		}
	}
	else
	{
		return EXIT_FAILURE;
	}

	/*
	 * Don't forget to release the objects in the array.
	 */
	for (i = 0; i < machineCnt; ++i)
	{
		IMachine* machine = machines[i];

		if (machine)
		{
			IMachine_Release(machine);
		}			
	}
	g_pVBoxFuncs->pfnArrayOutFree(machines);

	return 0;
}
