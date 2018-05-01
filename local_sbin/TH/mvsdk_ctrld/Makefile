DAEMON = mvsdk_ctrld
CLIENT = mvsdk_iface
DAEMON_INIT = "/etc/init.d/${DAEMON}"
DAEMON_SYSTEMD = "/lib/systemd/system/${DAEMON}.service"
help:
	@echo "usage: make <target>"
	@echo "target: "
	@echo -e "\tbuild 		- to build the daemon and client"
	@echo -e "\tinstall 	- needs root-permission to install"
	@echo -e "\tuninstall 	- needs root-permission to uninstall"
	@echo -e "\tdemo 		- a example using opencv to show image"
	@echo -e "\tclean 		- clean the generated binary files"

${DAEMON}: ${DAEMON}.c
	${CC} -o ${DAEMON} ${DAEMON}.c -lMVSDK -L /usr/local/lib/THLib/

${CLIENT}: ${CLIENT}.c
	${CC} -shared -fPIC -o lib${CLIENT}.so ${CLIENT}.c

build: ${DAEMON} ${CLIENT}

install:
	@cp lib${CLIENT}.so /usr/local/lib
	@chown root:root /usr/local/lib/lib${CLIENT}.so
	@chmod 644 /usr/local/lib/lib${CLIENT}.so
	@ldconfig
	@cp ${CLIENT}.h* /usr/local/include
	@chown root:root /usr/local/include/${CLIENT}.h*
	@cp ${DAEMON} /usr/local/sbin/TH/
	@chown root:root /usr/local/sbin/TH/${DAEMON}
	@cp systemd/${DAEMON} ${DAEMON_INIT}
	@chown root:root ${DAEMON_INIT}
	@chmod 544 ${DAEMON_INIT}
	@cp systemd/${DAEMON}.service ${DAEMON_SYSTEMD}
	@chown root:root ${DAEMON_SYSTEMD}
	@chmod 444 ${DAEMON_SYSTEMD}
	@systemctl daemon-reload
	@echo "Finish installation"
	@echo "Please run - \"systemctl start ${DAEMON}\" - to start the daemon"

uninstall:
	@rm -f /usr/local/lib/lib${CLIENT}.so
	@rm -f /usr/local/include/${CLIENT}.h*
	@rm -f /usr/local/sbin/TH/${DAEMON}
	@rm -f /etc/init.d/${DAEMON}.init.sh
	@rm -f /lib/systemd/system/${DAEMON}.service
	@echo "Finish uninstallation"

demo: demo.cpp
	${CXX} `pkg-config --cflags opencv` -o $@ $^ `pkg-config --libs opencv` -lmvsdk_iface

clean:
	rm -f demo libmvsdk_iface.so mvsdk_ctrld
