# USB Device Permissions (UDEV rules) for Linux

This document provides instructions on how to manually configure UDEV rules to grant the necessary USB device permissions for iDescriptor to interact with recovery devices on Linux.

**Important Note:** iDescriptor will look for the UDEV rules specifically at the path `/etc/udev/rules.d/99-idevice.rules`. If you place them elsewhere, iDescriptor will not recognize the configuration. You don't have to define the rules in this path, it just won't be detected by iDescriptor but recovery devices should still work if the rules are correctly defined and applied.

## Manual Configuration Steps

You can run the following commands step by step to set up UDEV rules and permissions. Replace `<your_username>` with your Linux username (you can run `whoami` to see it).

**1. Create the UDEV rules file**

This creates `/etc/udev/rules.d/99-idevice.rules` with the correct permissions for Apple USB devices so iDescriptor can detect it.

```sh
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="05ac", MODE="0666", GROUP="idevice"' | sudo tee /etc/udev/rules.d/99-idevice.rules > /dev/null
```

**2. Create the `idevice` group if it does not exist**

This checks whether the `idevice` group exists and creates it if needed.

```sh
getent group idevice || sudo groupadd idevice
```

**3. Add your user to the `idevice` group**

This gives your user access to devices owned by the `idevice` group.

```sh
sudo usermod -aG idevice <your_username>
```

**4. Reload UDEV rules**

This reloads UDEV so the new rules take effect immediately.

```sh
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### Log Out and Log Back In

For the group changes to take full effect, you **must** log out of your current session and log back in. This ensures that your user session correctly picks up the new group membership.

After logging back in, you can re-run the dependency check in iDescriptor to verify that the UDEV rules are now should be detected as installed (if you installed at the path `/etc/udev/rules.d/99-idevice.rules`).
