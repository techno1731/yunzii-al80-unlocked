#!/usr/bin/env python3
"""Talk to the AL80 display module and print what it answers.

    al80_probe.py listen [seconds]          print whatever the module sends unprompted
    al80_probe.py send <type> [data bytes]  send one command (hex), then print the reply

Needs our firmware (build 8 or later), which keeps the module's replies for raw HID 0x53.
"""
import sys
import time

import al80_screen as s


def drain(device):
    out = bytearray()
    while True:
        device.write([0x00, 0x53] + [0] * 63)
        r = device.read(64, 500)
        if not r or r[0] != 0x53:
            sys.exit("this firmware does not capture module replies")
        out += bytes(r[2:2 + r[1]])
        if r[62]:
            print(f"  ({r[62]} bytes lost to overflow)")
        if not r[63]:
            return bytes(out)


def frames(blob):
    """Split a byte stream into A5 5A packets where possible."""
    out, i = [], 0
    while i < len(blob):
        if blob[i:i + 2] == b"\xA5\x5A" and i + 7 <= len(blob):
            kind, length = blob[i + 2], (blob[i + 3] << 8) | blob[i + 4]
            crc_ok = s.crc16_modbus(blob[i + 2:i + 5]) == ((blob[i + 5] << 8) | blob[i + 6])
            body = blob[i + 7:i + 7 + length]
            out.append(f"type 0x{kind:02X} len {length} data [{body.hex(' ')}]" + ("" if crc_ok else " (bad crc)"))
            i += 7 + length
        else:
            j = blob.find(b"\xA5\x5A", i + 1)
            j = len(blob) if j < 0 else j
            out.append(f"raw [{blob[i:j].hex(' ')}]")
            i = j
    return out


def main():
    device = s.open_device()
    try:
        if len(sys.argv) > 1 and sys.argv[1] == "listen":
            drain(device)
            end = time.time() + float(sys.argv[2] if len(sys.argv) > 2 else 5)
            while time.time() < end:
                got = drain(device)
                for line in frames(got):
                    print(line)
                time.sleep(0.2)
        elif len(sys.argv) > 2 and sys.argv[1] == "send":
            kind = int(sys.argv[2], 16)
            data = [int(x, 16) for x in sys.argv[3:]]
            drain(device)
            for packet in (s.report(s.OP_ANNOUNCE, s.command(kind, data)), s.FINISH):
                device.write([0x00] + packet)
                device.read(64, 500)
            time.sleep(0.4)
            reply = drain(device)
            print(f"sent type 0x{kind:02X} data {data}: {len(reply)} bytes back")
            for line in frames(reply):
                print("  " + line)
        else:
            sys.exit(__doc__)
    finally:
        device.close()


if __name__ == "__main__":
    main()
