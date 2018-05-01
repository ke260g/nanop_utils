#!/bin/bash

# This is a script can be safely run
# on nanopc and nanopi with kernel 3.4.39 only,
# which are arm-linux-board produced by friendlyarm
#
# The script performs follow functionality
# to create the arm-linux-gcc* toolchain links under /bin,
# the source toolchain-bin is under /usr/bin,
# named as arm-linux-gnueabihf-*, which need to be renamed

if [[ `uname -r` != "3.4.39-s5p6818" ]] || [[ `whoami` != root ]] || [ ! -d /home/fa ]; then
echo you are not in the safe-running environment
exit
fi

ls -1 /usr/bin/arm-linux* | sed -e '/[0-9]$/d' > oldBin.txt

cp oldBin.txt newBin.txt

sed -i -e 's|/usr||' -e 's|gnueabihf-||' newBin.txt

# what else need sed to perform ?
# [1] add the `ln -sf'
# [2] made the multi tab to blank
# [3] make the multi blank a single one
#

diff -y oldBin.txt newBin.txt |\
	sed -e 's/|//' -e 's/^/ln -sf /' -e 's/\t\{1,\}/ /gs/ \{2,\}/ /g' |\
 	sh
rm oldBin.txt newBin.txt
