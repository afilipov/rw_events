Recording and Replay linux input events

https://github.com/afilipov/rw_events

How to build:

make

make clean     - Clean, but keep libraries and executables

make distclean - Cleanup all

Examples, how to use the tools:

1. Show help

	ev_record -h

2. Recording event to default location "tmp/events.bin"

	ev_record

3. Replay event from default location "tmp/events.bin"

	ev_replay

4. Recording event to specific location and filename

	ev_record -f mouse_move.rec

5. Replay recorded events from specific location and filename

	ev_replay -f mouse_move.rec
