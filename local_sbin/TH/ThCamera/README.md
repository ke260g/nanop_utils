# ThCamera Message Echo Daemon

### backgroud
 + listen a udp-port, any byte send to this port will cause a echo-back-message
 + message consist of two part:
    +  1. /etc/TH/ThCamera.conf.json, just copy the json `key:value` pair
    +  2. the rootfs storage of disk space
 + echo-back-message send back to the peer via udp

### project files description
+ ThCamera.c, daemon, bin-code will be placed in /usr/local/sbin/TH
+ ThCamera_ping, a client demo
+ systemd/ThCamera, will be placed in /etc/init.d
+ systemd/ThCamera.service, will be placed in /lib/systemd/system

### daemon management note
+ $> `/etc/init.d/ThCamera {start|stop|restart|status}`
+ $> `systemctl {start|stop|restart} ThCamera`
+ daemon's pid stores in /var/run/TH/ThCamera.pid
+ daemon's log stores in /var/log/TH/ThCamera.log
+ manually run daemon multi-times will must cause undefined bugs

### client usage example
+ ./ThCamera_ping 192.168.3.55
+ ./ThCamera_ping 127.0.0.1
