#!/usr/bin/env python3
"""Reconnect storm for the TCP serial console.

Repeatedly connects to the console, makes the guest produce output, and then
slams the socket shut with an RST while that output is still in flight.  Each
round is followed by three assertions:

  1. the emulator process is still alive,
  2. it is not holding more file descriptors than it did at the start, and
  3. a freshly connected client still gets the console banner.

Those correspond to three separate defects in CSerial: SIGPIPE was not
ignored, so a client could kill the emulator; a client socket was never
closed, so every reconnect leaked a descriptor until accept() failed with
EMFILE; and an accept() error was treated as a connection, after which the
device selected() on descriptor -1 forever and no client could ever be served
again.

Exit status 0 means all three held for every round.
"""

import argparse
import os
import socket
import struct
import sys
import time

BANNER = b"serial port"
# Enough typing that the guest has definitely written something we never read.
FLOOD = b"SHOW TIME\r\n" * 24


def pid_listening_on(port):
    """Pid of the process holding the listening socket on `port`.

    Matching on the port rather than on the process name matters: a machine
    running this can have more than one emulator alive, and name-matching picks
    either.  Returns None where the information is unavailable (macOS has no
    /proc), which the caller treats as "skip the descriptor check".
    """
    inodes = set()
    for path in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            with open(path, encoding="ascii") as fh:
                rows = fh.read().splitlines()[1:]
        except OSError:
            return None
        for row in rows:
            cols = row.split()
            # sl local_address remote_address state ... inode is field 9
            if len(cols) <= 9:
                continue
            try:
                local_port = int(cols[1].split(":")[1], 16)
            except (IndexError, ValueError):
                continue
            if cols[3] == "0A" and local_port == port:  # 0A = TCP_LISTEN
                inodes.add(cols[9])
    if not inodes:
        return None

    want = {f"socket:[{i}]" for i in inodes}
    try:
        pids = sorted(int(d) for d in os.listdir("/proc") if d.isdigit())
    except OSError:
        return None
    for p in pids:
        try:
            names = os.listdir(f"/proc/{p}/fd")
        except OSError:
            continue
        for name in names:
            try:
                target = os.readlink(f"/proc/{p}/fd/{name}")
            except OSError:
                continue
            if target in want:
                return p
    return None


def count_fds(pid):
    if pid is None:
        return None
    try:
        return len(os.listdir(f"/proc/{pid}/fd"))
    except OSError:
        return None


def alive(pid):
    return pid is None or os.path.isdir(f"/proc/{pid}")


def connect(host, port):
    s = socket.create_connection((host, port), timeout=10)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def read_banner(sock, seconds=10):
    """True when the console greets us.  A wedged console accepts the TCP
    connection and then says nothing at all, so this is the liveness test."""
    sock.settimeout(seconds)
    seen = b""
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        seen += chunk
        if BANNER in seen:
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=21003)
    ap.add_argument("--rounds", type=int, default=45)
    ap.add_argument("--settle", type=float, default=0.3,
                    help="seconds to let output pile up before closing")
    ap.add_argument("--tolerance", type=int, default=2,
                    help="file descriptors the emulator may gain over the run")
    args = ap.parse_args()

    pid = pid_listening_on(args.port)
    if pid is None:
        print(f"note: cannot inspect /proc, so the descriptor check is "
              f"skipped; liveness is still tested")

    print(f"target {args.host}:{args.port} (pid {pid}), {args.rounds} rounds")
    print("round   fds   result")

    baseline = count_fds(pid)
    for n in range(1, args.rounds + 1):
        try:
            s = connect(args.host, args.port)
        except OSError as e:
            print(f"{n:5d}   -     cannot connect: {e}")
            return fail("a reconnect storm must not make the console "
                        "unconnectable")

        try:
            s.settimeout(1.0)
            s.recv(4096)          # banner
            s.sendall(FLOOD)      # make the guest write at us
            time.sleep(args.settle)
            # SO_LINGER 0 sends an RST instead of a FIN.  That is the trigger,
            # not a trick: it is what makes the emulator's next send() fail,
            # which is how a client that has gone away actually presents.
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                         struct.pack("ii", 1, 0))
            s.close()
        except OSError:
            pass

        time.sleep(0.2)
        if not alive(pid):
            print(f"{n:5d}  DEAD   the emulator process is gone")
            return fail("a client disconnect killed the emulator "
                        "(SIGPIPE is not ignored)")

        fds = count_fds(pid)
        grew = "" if baseline is None or fds is None else \
            f"  (+{fds - baseline} since the start)"
        print(f"{n:5d} {fds if fds is not None else '-':>5}   ok{grew}")

        if baseline is not None and fds is not None and \
                fds - baseline > args.tolerance:
            return fail(f"the descriptor count grew from {baseline} to {fds} "
                        f"over {n} reconnects: client sockets are being "
                        f"leaked")

    time.sleep(1.0)
    try:
        s = connect(args.host, args.port)
        responded = read_banner(s)
        s.close()
    except OSError as e:
        print(f"\nfinal probe: cannot connect ({e})")
        return fail("the console stopped accepting connections")

    if not responded:
        return fail("the console accepted the final connection and sent "
                    "nothing: it is wedged")

    print(f"\nfinal probe: the console still greets a new client")
    print(f"PASSED: {args.rounds} reconnects, no crash, no leak, no wedge")
    return 0


def fail(why):
    print(f"\nFAILED: {why}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
