# Flashing

## Back up first

The bootloader can read the whole flash, firmware and saved settings together. Do this once
before the first flash; it is the only exact copy of your keyboard's factory firmware.

1. Unplug the keyboard, hold `Esc`, plug it in. Lights and screen stay off: that is the bootloader.
2. `dfu-util -l` should list `1eaf:0003`.
3. `dfu-util -d 1eaf:0003 -a 2 -U stock-backup.bin`

The read ends with a pipe error after 122,880 bytes. That is the end of the flash, and the file is
complete. Read it twice and compare the two files.

## Flash

```
firmware/build.sh
firmware/flash.sh
```

`flash.sh` first asks a running development build to enter the bootloader over raw HID. If the
keyboard is on stock firmware, or the request is refused, enter the bootloader by hand as above.

On the first boot after another firmware, saved settings are reset once and the keyboard restarts.

## Go back to stock

Enter the bootloader and write your backup: `dfu-util -d 1eaf:0003 -a 2 -D stock-backup.bin -R`.
The bootloader lives in the first 8 KB and is never written, so a bad flash can always be redone.

## Development builds

The universal keymap defines `AL80_DEV_HID_BOOT`, which lets any program on the computer put the
keyboard into the bootloader. Remove that define for a build you hand to someone else.
