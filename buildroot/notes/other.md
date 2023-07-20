# Logging

To save syslog logs from multiple boot sessions, the following trick can be
used:

```
::sysinit:/bin/sh -c "ln -sf /var/log/messages_$(date +%H-%m:%Y-%m-%d) /var/log/messages"
```
