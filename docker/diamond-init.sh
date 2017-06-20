#!/bin/sh
set -e
# Pay attention to the use of $* to suppress word splitting: http://mywiki.wooledge.org/Quotes#Expand_argument_lists
exec /usr/bin/socat TCP4-LISTEN:5555 EXEC:"$*"
