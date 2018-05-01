# MindVison SDK control daemon

### backgroud
+ the **raw**-mvsdk interface needs root-permission
+ the **raw**-mvsdk interface provides too many complex routines
+ C/S mode to simplify the usage and handle permission problem

### project files description
+ mvsdk_ctrld.c, daemon, bin-code will be placed in /usr/local/sbin/TH
+ mvsdk_ctrld.h, daemon and client ipc communication protocol
+ mvsdk_iface.c, client interface source, to build a shared-lib
+ mvsdk_iface.h, userspace header to use client interface
+ mvsdk_iface.hpp, simple c++ wrapper of mvsdk_iface.h
+ demo.cpp, a demo to use mvsdk_iface and opencv to cap and show image
+ systemd/mvsdk_ctrld, will be placed in /etc/init.d/
+ systemd/mvsdk_ctrld.service, will be placed in /lib/systemd/system

### daemon and interface management note
+ $> `/etc/init.d/mvsdk_ctrld {start|stop|restart|status}`
+ $> `systemctl {start|stop|restart|status} mvsdk_ctrld`
+ daemon's pid stores in /var/run/mvsdk_ctrld.pid
+ daemon's log stores in /var/log/mvsdk_ctrld.log
+ manually run daemon multi-times will must cause undefined bugs
+ mvsdk_iface.so, it's not safe to be used in multi-processes/threads
+ last successful process, using mvsdk_iface.so,
    stored its own pid in /tmp/mvsdk_iface.pid
