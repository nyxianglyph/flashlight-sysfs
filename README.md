# flashlight-sysfs

A flashlight provider for Sailfish OS that switches the torch through a
sysfs file.

Sailfish's flashlight switch talks to the session D-Bus service
`org.sailfish.flashlight.provider`. The stock implementation,
`droidmedia-flashlight`, asks the Android side to switch the torch
(`droid_media_camera_set_torch_mode`). On ports whose Android system image
has a libdroidmedia without that function the switch does nothing. If the
kernel exposes the torch in sysfs, this package can take its place: it
implements the same D-Bus interface (`toggleFlashlight`, `flashlightOn`,
`flashlightOnChanged`) and writes to the configured file.

## Configuration

`/etc/flashlight-sysfs.conf`:

    path=/sys/class/flashlight_core/flashlight/torchbrightness
    on=100
    off=0

The defaults are for the Xiaomi 14T (`degas`, MediaTek flashlight core with
an LM3644 driver), where the value is the torch current in mA.

`flashlight-sysfs-permissions.service` gives the `camera` group write access
to that file; the session user is a member of that group. Android's init
sets the ownership of such nodes itself when `sys.boot_completed` turns 1,
so the service waits for that property first.

## Building

In the Sailfish SDK, with `glib2-devel`:

    rpmbuild --build-in-place -bb rpm/flashlight-sysfs.spec

The package provides `sailfish-flashlight-provider` and replaces
`droidmedia-flashlight`.

## Status

Tested on the Xiaomi 14T with Sailfish OS 5.2.0.22: toggling over D-Bus
switches the LED on and off. The service exits 30 seconds after the torch
has been switched off and is started again by D-Bus on demand.
