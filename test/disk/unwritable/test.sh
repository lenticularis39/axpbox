#!/bin/bash

touch disk-unwritable.img
chmod 400 disk-unwritable.img

# Download the firmware
if [[ ! -f "cl67srmrom.exe" ]]; then
  wget 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'
fi

# Start AXPbox
../../../build/axpbox run | tee axp.log



chmod 700 disk-unwritable.img
rm disk-unwritable.img

diff axp_correct.log axp.log
