#!/bin/sh -eu

/usr/sbin/sshd -D >/dev/null 2>&1 &
exec /bin/bash
