# yunzii-al80-unlocked

Open firmware and tools for the YUNZII AL80 keyboard and its screen.

- **All three connections:** USB, 2.4G dongle and Bluetooth, chosen by the switch underneath.
- **Mac mode and Windows/Linux mode,** switched on the keyboard. Mac mode has a real Globe key.
- **Your own screen:** GIFs, several GIFs as one cycling medley, or pictures, from the command line.
- **Lighting that follows the screen,** a dark mode, and a real battery gauge.

[docs/status.md](docs/status.md) lists what is proven on hardware and what is only built.

## Use it

```
firmware/build.sh                              # build
firmware/flash.sh                              # flash over the bootloader
tools/al80_screen.py gif a.gif b.gif c.gif     # medley on the GIF page
tools/al80_screen.py gif strip.gif --where home
```

Back up your factory firmware before the first flash: [docs/flashing.md](docs/flashing.md).

## Keys

Hold the first key right of the space bar and press:

| Key | Does |
|---|---|
| `1` `2` `3` | Bluetooth slot: tap to connect, hold to pair |
| `4` / `5` | 2.4G dongle / USB |
| `6` | Dark mode |
| `7` | Rotate home page and GIF |
| `8` `9` `0` | Home page, picture, GIF |
| `A` / `S` | Mac mode / Windows and Linux mode |

## More

- [docs/protocol.md](docs/protocol.md): how the keyboard, screen and radio are spoken to.
- [docs/display-module.md](docs/display-module.md): what is known about the screen's closed module. Help wanted.

## Credits and licence

Started from [snackdriven/al80-lcd](https://github.com/snackdriven/al80-lcd). Protocol facts also
come from YUNZII's published B75 Pro source and [ZerO970/yunzii-display](https://github.com/ZerO970/yunzii-display).
Built on [QMK](https://qmk.fm) and [Vial](https://get.vial.today).

`firmware/` is GPL-2.0-or-later, as QMK requires. Everything else is MIT.

Not affiliated with YUNZII. Flash at your own risk.
