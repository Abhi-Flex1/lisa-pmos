#!/usr/bin/env python3
"""Probe /dev/qseecom_mobile (Xiaomi lisa fingerprint TA).

Run as root on the phone:

    sudo python3 qseecom_probe.py fpclisa            # fixed probe commands
    sudo python3 qseecom_probe.py fpclisa 05020000   # one custom request

The ioctl request uses the standard Linux layout:
    dir << 30 | size << 16 | type << 8 | nr
Swapping nr and size makes every ioctl return ENOTTY even though the driver
is fine.
"""
import ctypes
import fcntl
import os
import sys

NRBITS, TYPEBITS, SIZEBITS = 8, 8, 14
WRITE, READ = 1, 2


def ioc(direction, typ, nr, size):
    return (direction << (NRBITS + TYPEBITS + SIZEBITS)) | \
           (size << (NRBITS + TYPEBITS)) | (typ << NRBITS) | nr


class LoadApp(ctypes.Structure):
    _fields_ = [("app_name", ctypes.c_char * 64), ("app_id", ctypes.c_uint32)]


class SendCmd(ctypes.Structure):
    _fields_ = [("app_id", ctypes.c_uint32),
                ("req_len", ctypes.c_uint32),
                ("req_ptr", ctypes.c_uint64),
                ("rsp_len", ctypes.c_uint32),
                ("rsp_ptr", ctypes.c_uint64)]


LOAD_APP = ioc(READ | WRITE, ord("Q"), 1, ctypes.sizeof(LoadApp))
UNLOAD_APP = ioc(WRITE, ord("Q"), 2, 4)
SEND_CMD = ioc(READ | WRITE, ord("Q"), 3, ctypes.sizeof(SendCmd))


def send(fd: int, app_id: int, payload: bytes) -> bytes:
    req = (ctypes.c_ubyte * max(len(payload), 1)).from_buffer_copy(payload)
    rsp = (ctypes.c_ubyte * 256)()
    cmd = SendCmd(app_id=app_id,
                  req_len=len(payload), req_ptr=ctypes.addressof(req),
                  rsp_len=len(rsp), rsp_ptr=ctypes.addressof(rsp))
    fcntl.ioctl(fd, SEND_CMD, cmd)
    return bytes(rsp[:cmd.rsp_len])


def main() -> int:
    name = sys.argv[1] if len(sys.argv) > 1 else "fpclisa"
    payloads = ([bytes.fromhex(sys.argv[2])] if len(sys.argv) > 2 else
                [bytes([i & 0xff, (i >> 8) & 0xff]) + bytes(30)
                 for i in (0, 1, 2, 3, 4, 5, 6, 7, 0x10, 0x20)])

    fd = os.open("/dev/qseecom_mobile", os.O_RDWR)
    app = LoadApp()
    app.app_name = name.encode()
    fcntl.ioctl(fd, LOAD_APP, app)
    print(f"LOAD_APP({name!r}): app_id={app.app_id}")
    if not app.app_id:
        return 1

    for payload in payloads:
        rsp = send(fd, app.app_id, payload)
        print(f"req={payload.hex()} -> rsp_len={len(rsp)} "
              f"head={rsp[:16].hex()}")

    fcntl.ioctl(fd, UNLOAD_APP, ctypes.c_uint32(app.app_id))
    print(f"UNLOAD_APP({app.app_id})")
    os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
