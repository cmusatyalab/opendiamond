#!/bin/sh
set -e
# Pay attention to the use of $* below to avoid word splitting pitfall
exec /usr/bin/socat TCP4-LISTEN:5555 EXEC:"$*"
