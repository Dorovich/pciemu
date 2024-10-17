#!/bin/bash
# setup.sh : Setup qemu for building pciemu
#
# Should run this only once.
#
# SPDX-License-Identifier: GPL-2.0
#

printf "Initializing setup. After this, you may build QEMU.\n"

# Clone QEMU source
git submodule update --init

# Repository information
REPOSITORY_DIR=$(git rev-parse --show-toplevel)
REPOSITORY_NAME=$(basename $REPOSITORY_DIR)

# Edit original build files
echo "source $REPOSITORY_NAME/Kconfig" >> qemu/hw/misc/Kconfig
echo "subdir('$REPOSITORY_NAME')" >> qemu/hw/misc/meson.build

# Create symbolic links to device files
ln -s $REPOSITORY_DIR/src/hw/$REPOSITORY_NAME/ $REPOSITORY_DIR/qemu/hw/misc/

# Create symbolic link to the pciemu_hw.h include file
# This will avoid changing the meson files to be able to find this include
ln -s $REPOSITORY_DIR/include/hw/pciemu_hw.h $REPOSITORY_DIR/src/hw/$REPOSITORY_NAME/pciemu_hw.h

# Configure QEMU
cd qemu
./configure \
	--disable-bsd-user --disable-guest-agent --disable-gtk --disable-werror \
	--enable-curses --enable-slirp --enable-libssh --enable-vde --enable-virtfs \
	--target-list=riscv64-softmmu

printf "\nSetup finished. You may now build QEMU:\n\tcd qemu && make \$(nproc --ignore 2)\n"
