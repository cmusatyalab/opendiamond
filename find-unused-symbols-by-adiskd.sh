#!/bin/sh

nm adiskd/adiskd --defined-only -f p | grep ' [T] ' | cut -d " " -f 1 | sort -u > adiskdsyms.txt
nm lib/.libs/libopendiamond.a  --defined-only -f p | grep ' [T] ' | cut -d " " -f 1 | sort -u > libsyms.txt

diff -u libsyms.txt adiskdsyms.txt  | grep '^[-]' | less
