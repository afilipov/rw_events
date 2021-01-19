# rw_events
Recording and Replay linux input events

make

make clean     - Clean, but keep libraries and executables

make distclean - Cleanup all

Usage:

  sudo ./out/record -d /dev/input -o /tmp/events.bin
  
  sudo ./out/replay -d /dev/input -f /tmp/events.bin
