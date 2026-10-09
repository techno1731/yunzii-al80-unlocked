# The display module

The screen is a module with its own processor and firmware, wired to the keyboard's main chip by
a serial line (USART3, 921600 baud, 8N1). The keyboard relays packets to it and, with this
firmware, also records what it answers.

## What is known (9 Oct 2026)

- **It answers.** Every packet is acknowledged with a type `0x00` packet carrying a status byte.
  The module also sends a status packet about once a second while idle.
- **Firmware version 3.** A type `0x14` packet with no data is answered with `14 00 01 .. 03`.
- **Command set ends at `0x14`.** Types `0x15` to `0x1A`, present in the vendor's B75 Pro source,
  come back as status 5 then 2.
- **The home page is drawn by the module.** The keyboard only supplies values: connection, OS,
  lock states, battery, time and date.
- **Animation lives in three stored GIF slots.** Pictures are stored too, in a ring of slots.

## What is not known

- The module's processor, and whether its firmware can be read or replaced.
- Whether a stored picture is written to flash every time, which decides how often a
  keyboard-drawn page may be refreshed.
- The module's real playback speed for full-screen GIFs.
- Any drawing, partial-update or streaming command. None has been found.
- Whether packet types above `0x1A` do anything. They have not been tried: an unknown type could
  erase stored content or start a firmware update, so that scan is a deliberate step.

## How to look

`tools/al80_probe.py` sends one packet and prints the reply, or listens:

```
tools/al80_probe.py listen 5
tools/al80_probe.py send 14          # version
tools/al80_probe.py send 0B          # home page
```

## Ways forward

1. **Open the keyboard and read the chip markings** on the screen board. A part number decides
   everything else: whether a programmer can read it, whether a known bootloader exists.
2. **Measure what the keyboard can push directly.** The main chip can send a full frame in about
   a third of a second at the serial line's speed, with no USB in the way.
3. **Test picture persistence** by powering the module down and up (the firmware controls its
   power pin) and checking whether an uploaded picture survives.
4. **Scan the unknown packet types,** accepting the risk above.
