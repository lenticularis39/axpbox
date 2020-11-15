#!/bin/bash

touch disk-unwritable.img
chmod 400 disk-unwritable.img

# Download the firmware
if [[ ! -f "cl67srmrom.exe" ]]; then
  wget 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'
fi

# Start AXPbox
if [[ -f ../../../build/axpbox ]]; then
  ../../../build/axpbox run | tee axp.log
elif [[ -f ../../../../build/axpbox ]]; then
  ../../../../build/axpbox run | tee axp.log;
else
   ../../build/axpbox run | tee axp.log
fi

chmod 700 disk-unwritable.img
rm disk-unwritable.img

sed -i -e 's$/[^ ]*/DiskFile.cpp$DiskFile.cpp$g' \
    -e 's/line [0-9]*/line L/g' -e '/$Id/d' axp.log

echo -n -e '\033[1;31m'
diff -c axp_correct.log axp.log && echo -e '\033[1;32mdiff clean\033[0m'
result=$?
echo -n -e '\033[0m'

rm -f axp.log cl67* *.rom
exit $result
