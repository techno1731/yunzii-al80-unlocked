#!/usr/bin/env python3
"""Read the AL80 firmware diagnostic (raw HID 0x4C) and print it."""
import sys
import hid

IDS = [(0x05AC, 0x029C), (0x28E9, 0x30AF)]


def open_raw():
    for vid, pid in IDS:
        for d in hid.enumerate(vid, pid):
            if d["usage_page"] == 0xFF60 and d["usage"] == 0x61:
                h = hid.device()
                h.open_path(d["path"])
                return h
    sys.exit("AL80 raw HID interface not found")


h = open_raw()
h.write([0x00, 0x4C] + [0] * 63)
r = h.read(64, 1000)
h.close()
if not r or r[0] != 0x4C:
    sys.exit(f"no diagnostic reply: {r[:8] if r else r}")
o = r[1:]  # o[i] matches out[i] in al80_wireless_debug
w = lambda i: (o[i] << 8) | o[i + 1]
modes = {0: "USB", 1: "BT1", 2: "BT2", 3: "BT3", 4: "2.4G"}
sw = {0: "wired", 1: "BT", 2: "2.4G", 3: "invalid"}
print(f"build            {r[61]}")
print(f"mode             {modes.get(o[0], o[0])}   link up: {bool(o[1])}")
print(f"radio bytes      tx {w(2)}  rx {w(4)}  last rx {bytes(o[6:10]).hex(' ')}")
print(f"module says mode {o[46]}   stored boot byte 0x{o[45]:02X}   boot flags 0x{o[47]:02X}")
frames = [tuple(o[27 + i * 3: 30 + i * 3]) for i in range(o[25])]
print(f"radio frames     {frames}")
print(f"switch pins      C14(BT)={'high' if o[48] & 1 else 'LOW'}  C15(2.4G)={'high' if o[48] & 2 else 'LOW'}  -> {sw[o[49]]}  seen wireless: {bool(o[48] & 4)}")
print(f"host suspended   {bool(o[48] & 8)}   lock LEDs from radio 0x{o[50]:02X}")
print(f"battery          raw {(r[52] << 8) | r[53]}  vref {(r[54] << 8) | r[55]}  {(r[56] << 8) | r[57]} mV  {r[58]}%  status {r[59]}")
print(f"plugged          {bool(r[60] & 1)}   idle: {bool(r[60] & 2)}")
