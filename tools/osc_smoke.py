#!/usr/bin/env python3
"""Minimal OSC client for poking a running kv4d (no dependencies).

  python3 tools/osc_smoke.py                 # run the smoke test against port 7000
  python3 tools/osc_smoke.py 7100            # different kv4d -inport
  python3 tools/osc_smoke.py 7000 /kv4d/view/mode 2 1.5   # send one message
"""
import socket
import struct
import sys
import time


def _pad(b):
    return b + b"\0" * (4 - len(b) % 4)


def message(address, *args):
    tags, data = ",", b""
    for a in args:
        if isinstance(a, int):
            tags += "i"; data += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"; data += struct.pack(">f", a)
        else:
            tags += "s"; data += _pad(str(a).encode())
    return _pad(address.encode()) + _pad(tags.encode()) + data


def parse(packet):
    end = packet.index(b"\0")
    address = packet[:end].decode()
    i = (end + 4) & ~3
    end = packet.index(b"\0", i)
    tags = packet[i + 1:end].decode()
    i = (end + 4) & ~3
    args = []
    for t in tags:
        if t == "i":
            args.append(struct.unpack(">i", packet[i:i + 4])[0]); i += 4
        elif t == "f":
            args.append(struct.unpack(">f", packet[i:i + 4])[0]); i += 4
    return address, args


def arg(s):
    for conv in (int, float):
        try:
            return conv(s)
        except ValueError:
            pass
    return s


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 7000
    out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    send = lambda *m: out.sendto(message(*m), ("127.0.0.1", port))

    if len(sys.argv) > 2:
        send(sys.argv[2], *[arg(a) for a in sys.argv[3:]])
        return

    replies = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    replies.bind(("127.0.0.1", 57121))
    replies.settimeout(3.0)

    send("/kv4d/world/preset", "brain")
    send("/kv4d/view/mode", 1, 0.5)
    send("/kv4d/view/rotate", "yw", 0.3)
    send("/kv4d/world/inject", 0.5, 0.5, 0.5, 0.5, 0.15, 0.6)
    send("/kv4d/world/query/alive", 1)
    try:
        for _ in range(3):
            address, args = parse(replies.recv(1024))
            print("received", address, args)
    except socket.timeout:
        print("no reply from kv4d on 57121")
        sys.exit(1)
    finally:
        send("/kv4d/world/query/alive", 0)


if __name__ == "__main__":
    main()
