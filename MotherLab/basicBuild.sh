#!/bin/bash

# these strings need to be 'echo' with '-e' option
CONFIRM_PROMPT="[\e[32;1mYes\e[0m/\e[31;1mNo\e[0m]\c"
RE_CONFIRM="\e[31;40mPlease input 'yes' or 'no'.\e[0m"

function first_build {
	# edit /etc/hosts
	sed -i -e '/127\.0\.0\.1/s|localhost|NanoPi3|' /etc/hosts

	# edit /etc/sudoers, allow the fa(user) NOPASSWD
	# change line 	fa ALL=(ALL:ALL) ALL
	# to 		fa ALL=(ALL) NOPASSWD: ALL
	chmod u+w /etc/sudoers
	sed -i -e '/^fa/{/ALL:ALL/s|).*$||}' /etc/sudoers # fa ALL=(ALL:ALL
	sed -i -e '/^fa/{/ALL:ALL/s|:|) NOPASSWD: |}' /etc/sudoers
	chmod u-w /etc/sudoers

	# build basic network
	apt-get update
	apt-get -y install network-manager network-manager-gnome
	systemctl disable networking
	systemctl enable NetworkManager.service
    # fix wlan0 > wlan1 > wlan2 bugs
    rm /etc/udev/rules.d/70-persistant-net.rules

	# edit /etc/network/interfaces, comment all interfaces
	sed -i -e '/eth0/,${/^[^#]/s|^|# |}' /etc/network/interfaces

	# edit passwd, allow fa to be a superuser
	sed -i -e '/^fa:/s|1000:1001|0:0|' /etc/passwd
	reboot
}

function second_build {
	# package network service
	PKG_LIST[1]="vsftpd ncftp rpcbind nfs-kernel-server filezilla"
	# package cli utils
	PKG_LIST[2]="pinfo ranger htop dstat trash-cli tmux usbutils"
	# package develop kits
	PKG_LIST[3]="qt5-default qtcreator git cppman zsh i3"
	# package chinese support
	PKG_LIST[4]="fcitx fcitx-table-all"
	# package basic libraries
	PKG_LIST[5]="libncurses5-dev libusb-1.0-0-dev"
	# package terminal, as the finish mark
	PKG_LIST[6]="sakura"

	apt-get install -y `echo ${PKG_LIST[@]}`

	systemctl enable vsftpd.service
	systemctl enable nfs-kernel-server
	usermod -G tty,dialout,sys,bin,adm,ftp,uucp -a fa
	echo "@ifconfig eth0 192.168.5.100"

QT5INPUTLIB=/usr/lib/arm-linux-gnueabihf/qt5/plugins/platforminputcontexts
QTCREATORLIB=/usr/lib/arm-linux-gnueabihf/qtcreator/plugins
	cp -a ${QT5INPUTLIB} ${QTCREATORLIB}

	# edit the Desktop to a 'standard look'
}

# main entry
if [[ `whoami` != "root" ]]; then
	echo Please run as root
	exit
fi

dpkg -l | grep network-manager | grep -q "^ii"
if [[ `echo $?` == 1 ]]; then
	# first_build illustration
	echo -e ">>> \e[35;40mfirst_build\e[0m will perform a initial build.\n"
	echo -e "\e[32;40m>>> These are the actions that would be performed:"
	echo -e " \e[33;1m*\e[0m make user fa to a superuser."
	echo -e " \e[33;1m*\e[0m handle basic network issue."
	echo -e " \e[33;1m*\e[0m automatically reboot after finish."
	echo
	while true; do	# start first_build
		echo -e "Would you like to \e[35;40mfirst_build\e[0m? \c"
		echo -e ${CONFIRM_PROMPT}
		read CONFIRM
		CONFIRM=$(echo $CONFIRM | tr '[a-z]' '[A-Z]')
		if [[ ($CONFIRM == 'Y') || ($CONFIRM == 'YES') ]]; then
			echo Start Building
			first_build
			break
		fi
		if [[ ($CONFIRM == 'N') || ($CONFIRM == 'NO') ]]; then
			echo Cancel Building
			break
		fi
		echo -e ${RE_CONFIRM}
	done
else if [[ `dpkg -l | grep sakura | awk '{print $1}'` != `echo ii` ]]; then
	# second_build illustration
	echo -e ">>> \e[35;40msecond_build\e[0m will perform a basic build.\n"
	echo -e "\e[32;40m>>> These are the actions that would be performed:"
	echo -e " \e[33;1m*\e[0m build a completely qt5 development environment."
	echo

	while true; do # start second_build
		echo -e "Would you like to \e[35;40msecond_build\e[0m? \c"
		echo -e ${CONFIRM_PROMPT}
		read CONFIRM
		CONFIRM=$(echo $CONFIRM | tr '[a-z]' '[A-Z]')
		if [[ ($CONFIRM == 'Y') || ($CONFIRM == 'YES') ]]; then
			echo Start Building
			second_build
			break
		fi
		if [[ ($CONFIRM == 'N') || ($CONFIRM == 'NO') ]]; then
			echo Cancel Building
			break
		fi
		echo -e ${RE_CONFIRM}
	done
	fi
fi
