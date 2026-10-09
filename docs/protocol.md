# Protocol reference

Facts about how the AL80 is spoken to, as used by the code in this repository.

## USB

| Identity | When |
|---|---|
| `28E9:30AF` | Stock firmware, and this firmware in Windows/Linux mode |
| `05AC:029C` | This firmware in Mac mode (macOS honours the Fn/Globe key only from an Apple identity) |
| `1EAF:0003` | Bootloader (stm32duino), DFU alt 2 writes from `0x08002000` |

Vendor traffic uses the raw HID interface, usage page `0xFF60`, usage `0x61`, 64-byte reports.
Only one program can hold it at a time.

## Screen reports

Every screen operation is a run of 64-byte reports:

```
[0] opcode      0x40 announce, 0x41 data, 0x42 finish
[1..2] offset   little-endian byte offset of this chunk
[3] length      payload bytes in this report (at most 56)
[4..5] checksum 16-bit sum of bytes 0..3 and the payload, little-endian
[6] 0           the keyboard writes 0x55 here in its echo as an acknowledgement
[7..] payload   forwarded to the display module unchanged
```

The finish report is the constant `42 00 00 38 7A`.

## Display module packets

The payload carries packets for the display module, which the keyboard also sends on its own:

```
A5 5A <type> <length hi> <length lo> <crc hi> <crc lo> <data...>
```

The CRC is CRC-16/MODBUS over type and length. Data longer than one report follows in data reports.

| Type | Name | Data | Effect |
|---|---|---|---|
| `0x00` | reply | 1 byte | Sent by the module: 1 ok, 2 timeout, 3 length, 4 type, 5 header, 6 checksum, 7 size |
| `0x01` | connection | 0 USB, 1-3 Bluetooth slot, 4 2.4G | Home page icon |
| `0x02` | OS | 0 Windows, 1 Mac | Home page icon |
| `0x03` `0x04` `0x05` | Caps, Num, GUI lock | 0 or 1 | Home page icons |
| `0x06` | battery level | 0-100 | Home page gauge |
| `0x07` | battery state | 0 discharging, 1 charging, 2 full | Home page gauge |
| `0x08` | light mode | index | Accepted; effect not yet observed |
| `0x09` | time | hour, minute, second | Sets the clock |
| `0x0A` | date | year, weekday 1-7, month, day | Sets the date |
| `0x0B` | home | none | Show the home page |
| `0x0C` | add picture | length = 30720, then pixels | Store a still picture and show it |
| `0x0D` | next picture | none | Advance to the next stored picture |
| `0x0E` | delete picture | none | Delete a stored picture |
| `0x0F` | GIF | none | Show the GIF page |
| `0x10` | event | varies | Announces a picture (`01`) or a GIF frame (`02`, slot, index) |
| `0x11` | frame length | length, then pixels | One GIF frame |
| `0x12` `0x13` | GIF start and end | slot, then 0 / frame count / fps | Brackets a GIF upload |
| `0x14` | version | none | The module answers with its firmware version |

Types `0x15` to `0x1A` exist in the vendor's B75 Pro source (time format, typed letters) and are
rejected by the AL80's module, firmware version 3.

## Pixels

RGB565, high byte first, row by row. The panel is 96 x 160.

## Pictures

```
announce  type 0x10, data 01            then wait 300 ms
data      type 0x0C, length 30720       then wait 30 ms
data      30720 pixel bytes in 56-byte chunks, offsets running from 0
finish
```

Both waits are needed or the picture is received and never shown.

## GIFs

Three slots: `0` startup animation (96x160, 64 frames), `1` GIF page (96x160, 160 frames),
`2` home page strip (96x64, 42 frames). One frame rate for the whole GIF.

```
announce  type 0x12, data [slot, 0]
data      type 0x13, data [slot, 0]
per frame:
  data    type 0x10, data [02, slot, frame index]     wait 3 s on frames 0, 16, 32..., else 30 ms
  data    type 0x11, length = frame bytes
  data    pixels in 1024-byte banks; offsets restart in every bank; wait 30 ms after each bank
data      type 0x12, data [slot, frame count]
data      type 0x13, data [slot, fps]
finish
```

## Radio module

The radio is a coprocessor on USART1 at 460800 baud. Frames are `55 <length> <body>`.

| Direction | Frame | Meaning |
|---|---|---|
| to radio | `55 14 00 <mode> <name, 16 bytes> ...` sent twice | Connect in a mode: 1-3 Bluetooth slot, 4 2.4G |
| to radio | `55 03 00 <mode> 01` sent twice | Pair in a mode |
| to radio | `55 02 00 00` | Stop |
| to radio | `55 09 01 <8-byte keyboard report>` | Keys |
| to radio | `55 03 <report id> <usage lo> <usage hi>` | System (id 3) or consumer (id 4) key |
| to radio | `55 02 09 <percent>` | Battery level for the host |
| from radio | `55 03 00 <mode> <state>` | Link: 0 means up |
| from radio | `55 03 01 <mode> <leds>` | Host lock LEDs |
| from radio | `55 03 02 <mode> AA or BB` | 2.4G host suspended or resumed |

Sixty zero bytes and a 350 ms wait wake the radio before a mode command. Reports are spaced 8 ms
apart on Bluetooth and 2 ms on 2.4G. The mode switch under the keyboard pulls `C14` low for
Bluetooth and `C15` low for 2.4G.

## Firmware raw HID commands

| Opcode | Does |
|---|---|
| `0x43`-`0x48` | Palette and side bar colour (from the upstream firmware) |
| `0x49` | Per-key colour stream from the host |
| `0x4C` | Diagnostics: mode, radio counters, switch pins, battery, build number |
| `0x4E` `BOOT` | Development builds: enter the bootloader |
| `0x4F` | Battery sample history |
| `0x50` / `0x51` | Store / read the backlight colour list for the GIF page |
| `0x53` | Drain the display module's replies |
