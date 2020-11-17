#!/bin/bash
export LC_CTYPE=C
export LANG=C
export LC_ALL=C

# Download the firmware
wget 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'

# Start AXPbox
if [[ -f ../../../build/axpbox ]]; then
  ../../../build/axpbox run &
  AXPBOX_PID=$!
else # Travis
  ../../build/axpbox run &
  AXPBOX_PID=$!
fi

# Wait for AXPbox to start
sleep 3

# Connect to terminal
nc -t 127.0.0.1 21000 | tee axp.log &
NETCAT_PID=$!

# Wait for the last line of log to become P00>>>
timeout=300
while true
do
  if [ $timeout -eq 0 ]
  then
    echo "waiting for SRM prompt timed out" >&2
    exit 1
  fi

  if [ "$(tail -n 1 axp.log | tr -d '\0')" == "P00>>>"  ]
  then
    echo
    break
  fi

  sleep 1
  timeout=$(($timeout - 1))
done

kill $NETCAT_PID
kill $AXPBOX_PID

echo -n -e '\033[1;31m'
diff -c axp_correct.log axp.log && echo -e '\033[1;32mdiff clean\033[0m'
result=$?
echo -n -e '\033[0m'

rm -f axp.log cl67* *.rom
exit $result
