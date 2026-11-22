#!/bin/sh
# Give the session user's "camera" group write access to the torch file.
#
# Android's init sets the ownership of such nodes itself, typically in an
# "on property:sys.boot_completed=1" action, so this has to come after that
# and is repeated for a while in case something sets it again.
f=$(sed -n 's/^path=//p' /etc/flashlight-sysfs.conf)
[ -n "$f" ] || exit 0

i=0
while [ $i -lt 150 ]; do
    [ -e "$f" ] && [ "$(getprop sys.boot_completed 2>/dev/null)" = 1 ] && break
    sleep 2
    i=$((i+1))
done

n=0
while [ $n -lt 12 ]; do
    if [ -e "$f" ]; then
        chgrp camera "$f"
        chmod 0664 "$f"
    fi
    sleep 5
    n=$((n+1))
done
