DAEMON = ThCamera
CLIENT = ThCamera_ping
DAEMON_INIT = "/etc/init.d/${DAEMON}"
DAEMON_SYSTEMD = "/lib/systemd/system/${DAEMON}.service"

help:
	@echo "usage: make <target>"
	@echo "target: "
	@echo -e "\tbuild 		- to build the daemon and client"
	@echo -e "\tinstall 	- needs root-permission to install"
	@echo -e "\tuninstall 	- needs root-permission to uninstall"
	@echo -e "\tclean 		- clean the generated binary files"

JSON-C_CFLAGS=`pkg-config --cflags json-c`
JSON-C_LDFLAGS=`pkg-config --libs json-c`

${DAEMON}: ${DAEMON}.c
	${CC} ${JSON-C_CFLAGS} -o ${DAEMON} ${DAEMON}.c ${JSON-C_LDFLAGS}
${CLIENT}: ${CLIENT}.c
	${CC} -o ${CLIENT} ${CLIENT}.c -lrt

build: ${DAEMON} ${CLIENT}

install:
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
	@rm -f /usr/local/sbin/TH/${DAEMON}
	@rm -f /etc/init.d/${DAEMON}.init.sh
	@rm -f /lib/systemd/system/${DAEMON}.service
	@echo "Finish uninstallation"

clean:
	rm -f ThCamera_ping ThCamera
