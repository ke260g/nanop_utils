#!/bin/bash
rootfs_dev=/dev/mmcblk1p1
backup_dev=/dev/mmcblk1p4
rootfs=/mnt/rootfs
backup=/mnt/backup

rm /etc/udev/rules.d/70-persistent-net.rules
umount ${rootfs_dev} &>/dev/null
umount ${backup_dev} &>/dev/null
mkdir ${rootfs} &>/dev/null
mkdir ${backup} &>/dev/null

mount ${rootfs_dev} ${rootfs}
mount ${backup_dev} ${backup}
rm -rf ${backup}/*
rm -f ${rootfs}/debian/rootfs.img

echo "start to backup rootfs"
mkdir ${backup}/{boot,mnt,proc,sys} &>/dev/null
mkdir -p ${backup}/dev/{pts,shm} &>/dev/null
mkdir -p ${backup}/run/{lock,network} &>/dev/null
mkdir -p ${backup}/media/{fa,root} &>/dev/null
touch ${backup}/run/utmp &>/dev/null
chown -R root:root ${backup}
cp -a {/bin,/sbin,/lib,/usr,/home,/etc} ${backup}
cp -a {/var,/srv,/root,/opt} ${backup}

echo "start to build rootfs.img"
#make_ext4fs -l 7G -s -L linux ${rootfs}/debian/rootfs.img ${backup}

echo "release working buffer"
#rm -rf ${backup}/*
umount ${rootfs_dev}
umount ${backup_dev}

rm -rf ${backup}
rm -rf ${rootfs}
exit
