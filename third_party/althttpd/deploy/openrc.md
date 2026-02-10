Setup For Althttpd for OpenRC
==============================

This page describes setting up althttpd as a service using [OpenRC][],
one of the several available "init" systems in the Unix ecosystem.

Create the file `/etc/init.d/althttpd` with content like this:

>
```
#!/sbin/openrc-run
name=$RC_SVCNAME
description="The althttpd web server"
command=/usr/local/bin/althttpd
command_args="-root /home/www
              -port 80
              -logfile /logs/http.log
              -tls-port 443
              -certdir /etc/letsencrypt/live/main"
command_background=true
pidfile="/run/${RC_SVCNAME}.pid"
depend() {
  need net
}
```

Adjust the flags to suit your system, noting that paths for _some_ of
althttpd's path-valued flags must be resolvable from within the chroot
jail, e.g.  `-logfile /logs/http.log` in the above example really
means `/home/www/logs/http.log`.  Contrariwise, the `-root`, `-cert`,
and `-pkey` flags are resolved before entering the chroot jail.

To register that as an OpenRC service, do:

>
```
rc-update add althttpd default
```

To start it immediately (rather than waiting until the next reboot):

>
```
rc-service althttpd start
```

That's all there is to it.

[OpenRC]: https://github.com/OpenRC/openrc
