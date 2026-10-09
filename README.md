# yunzii-al80-unlocked

Open firmware and tools for the YUNZII AL80 keyboard and its screen.

The firmware makes the AL80 behave like a Mac keyboard without giving up anything the stock
firmware does, and the tools put your own animations on the screen from the command line.

## What you get

- **Every connection works:** USB (including through a monitor KVM hub), the 2.4G dongle and
  Bluetooth, chosen by the switch under the keyboard.
- **Mac mode and Windows/Linux mode,** switched on the keyboard. In Mac mode the board reports a
  real Fn/Globe key, so Globe shortcuts and Fn + arrows work, and the top row sends media keys.
- **Media keys over wireless,** a real battery gauge, and Caps Lock state relayed from the radio.
- **A screen you control:** upload GIFs, several GIFs as one cycling medley, or still pictures.
  The key backlight can take each scene's colour in step with the animation.
- **Lighting and screen keys:** solid managed lighting, a dark mode, automatic rotation between
  the home page and the GIF page.
- **Flashing without touching the keyboard** while a development build is loaded.

What is proven on hardware and what is only built is tracked in [docs/status.md](docs/status.md).
Read it before relying on a feature.

## Layout

| Path | What it holds | Licence |
|---|---|---|
| `firmware/` | QMK keyboard source, build and flash scripts, device-free tests | GPL-2.0-or-later |
| `tools/` | Screen uploader, module probe, diagnostics reader, tests | MIT |
| `screen/` | Scripts that draw the animations, and the rendered GIFs | MIT |
| `docs/` | Protocol reference, findings on the screen module, status | MIT |

## Quick start

Build and flash (macOS or Linux, needs a vial-qmk checkout, `arm-none-eabi-gcc`, `dfu-util`):

```
firmware/build.sh            # builds the universal keymap
firmware/flash.sh            # writes it over the stm32duino bootloader
firmware/test/run.sh         # device-free tests
```

The first flash from stock firmware needs the bootloader entered by hand: unplug, hold `Esc`,
plug in. Back up the whole flash first; see [docs/flashing.md](docs/flashing.md).

Put something on the screen (needs Python with `hidapi` and `Pillow`, keyboard on USB):

```
tools/al80_screen.py gif a.gif b.gif c.gif     # medley on the GIF page, backlight follows
tools/al80_screen.py gif strip.gif --where home
tools/al80_screen.py picture photo.png
tools/al80_screen.py view home|gif|picture
```

## Keys

The function-layer key is the first key right of the space bar. Hold it and press:

| Key | Does |
|---|---|
| `1` `2` `3` | Bluetooth slot: tap to connect, hold to pair |
| `4` / `5` | 2.4G dongle / back to USB |
| `6` | Dark mode: screen and all lights off, or back on |
| `7` | Screen alternates the home page and the GIF page |
| `8` `9` `0` | Home page, picture, GIF page |
| `A` / `S` | Mac mode / Windows and Linux mode (the keyboard restarts) |
| Right Shift, held 3 s | Enter the bootloader |

## How the screen works

The screen is a separate module with its own closed firmware. The keyboard can send it status
values, still pictures, stored GIFs and view switches, and nothing else is known to work yet.
[docs/protocol.md](docs/protocol.md) describes the wire format and
[docs/display-module.md](docs/display-module.md) records what the module answers when asked,
including the open questions. Help opening that module is welcome.

## Credits

- [snackdriven/al80-lcd](https://github.com/snackdriven/al80-lcd): the original reverse engineering
  of the screen's HID protocol and the first custom QMK firmware for this board. The keyboard
  source in `firmware/keyboard` started from that GPL firmware and still carries its copyright.
- [ZerO970/yunzii-display](https://github.com/ZerO970/yunzii-display): the parallel effort on the
  B75 Pro Max, whose call for help on the screen module this project also answers to.
- YUNZII's published QMK source for the B75 Pro, mirrored in
  [ArgentStonecutter/keyboards](https://github.com/ArgentStonecutter/keyboards), which documents
  the radio and screen protocols.
- [QMK](https://qmk.fm) and [Vial](https://get.vial.today).

## Licence

Everything outside `firmware/` is under the MIT licence in [LICENSE](LICENSE). The firmware is a
QMK keyboard and derives from GPL code, so `firmware/` is under GPL-2.0-or-later, see
[firmware/LICENSE](firmware/LICENSE).

Not affiliated with or endorsed by YUNZII. Flashing custom firmware is at your own risk.
