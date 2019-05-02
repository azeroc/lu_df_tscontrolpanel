# TSControlPanel

This project allows emulating touch-signals on VirtualBox virtual machine. They are captured from remote Raspberry Pi device using popular tslib library.

## Building and dependencies

For building it is best to use Visual Studio 2019+ and open this directory as CMake project. Make sure to copy/rename CMakeSettings.json.template as CMakeSettings.json and fill required fields for building to work.

Dependencies for TSControlNode on remote machine (typically Raspberry Pi running Rasbian OS):
- tslib (https://github.com/kergoth/tslib)

Dependencies for TSControlServer on remote machine (typically decent/powerful Linux server):
- VirtualBox 5.2.26

Currently the project only supports Linux (TSControlServer) and Raspberry Pi (TSControlNode) distros. 

## Upgrading VirtualBox

When deciding to upgrade (or downgrade) VirtualBox installation on the Linux server machine, you need to replace TSControlServer/vbox_api/* directories with the new version's SDK directories. These are:
- sdk/bindings/c/glue
- sdk/bindings/c/include
- sdk/bindings/xpcom/lib
- sdk/bindings/xpcom/include
