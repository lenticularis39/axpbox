#!/bin/bash
#
# Serial-console reconnect test.
#
# Starts a diskless, networkless emulator whose only interesting device is the
# TCP serial console, then hammers that console: repeated connects, output
# requested and never read, and the socket slammed shut with an RST while that
# output is in flight.  See reconnect.py for what each round asserts.
#
# Everything runs against a throwaway configuration on port 21003, so this can
# run alongside a live instance and cannot damage a disk image.

# Download the firmware
if [[ ! -f "cl67srmrom.exe" ]]; then
  wget 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'
fi

# Locate the binary (the same three places the other tests try).  Set AXPBOX in
# the environment to point at a different build -- handy for confirming that this
# test actually fails against an unfixed binary.
if [[ -z "$AXPBOX" ]]; then
  if [[ -f ../../../build/axpbox ]]; then
    AXPBOX=../../../build/axpbox
  elif [[ -f ../../../../build/axpbox ]]; then
    AXPBOX=../../../../build/axpbox
  else
    AXPBOX=../../build/axpbox
  fi
fi

if [[ ! -x "$AXPBOX" ]]; then
  echo "axpbox binary not found; build it before running the tests" >&2
  exit 1
fi

# A low descriptor limit is what makes the descriptor leak fail here in seconds
# rather than after the ~1000 reconnects it would otherwise take to exhaust the
# default limit.  40 leaves the emulator plenty to start with (it sits at 4-5
# descriptors when idle) and is reached well inside the round count below.
( ulimit -n 40; exec "$AXPBOX" run es40.cfg ) > axp.log 2>&1 &
AXPBOX_PID=$!

# Wait for the console to be listening.  The emulator creates the listening
# socket during device init and then blocks waiting for its first client, so
# this is also what lets the guest start.
PORT=21003
waited=0
while true; do
  if (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then
    exec 3<&- 3>&-
    break
  fi
  if ! kill -0 $AXPBOX_PID 2>/dev/null; then
    echo "axpbox exited before its console came up:" >&2
    tail -20 axp.log >&2
    rm -f axp.log cl67* *.rom
    exit 1
  fi
  if [ "$waited" -ge 60 ]; then
    echo "timed out waiting for the console on port $PORT" >&2
    tail -20 axp.log >&2
    kill $AXPBOX_PID 2>/dev/null
    rm -f axp.log cl67* *.rom
    exit 1
  fi
  sleep 1
  waited=$((waited + 1))
done

python3 reconnect.py --port $PORT --rounds 45
result=$?

kill $AXPBOX_PID 2>/dev/null
wait $AXPBOX_PID 2>/dev/null

if [ "$result" -ne "0" ]; then
  echo "--- last lines of the emulator log ---" >&2
  tr -d '\000' < axp.log | tail -20 >&2
fi

rm -f axp.log cl67* *.rom
exit $result
