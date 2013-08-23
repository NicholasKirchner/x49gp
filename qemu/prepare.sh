# get/update QEMU

# export CVS_RSH="ssh"
# cvs -z3 -d:pserver:anonymous@cvs.savannah.nongnu.org:/sources/qemu co -r "release_0_9_0" qemu

rm -rf qemu
tar xzvf qemu-0.9.0.tar.gz
mv qemu-0.9.0 qemu

# patch qemu sources
cd qemu

#qemu hotfix for qcow2
patch -p0 -u < ../patches/qemu-0.9.0-qcow2.diff

#qemu gcc4 patches
patch -p1 -u < ../patches/qemu-0.9.0-gcc4.patch
patch -p1 -u < ../patches/qemu-0.7.2-dyngen-check-stack-clobbers.patch
patch -p1 -u < ../patches/qemu-0.7.2-gcc4-opts.patch
patch -p1 -u < ../patches/qemu-0.8.0-gcc4-hacks.patch

#qemu OS X86 patches
patch -p1 -u < ../patches/qemu-0.9.0-enforce-16byte-stack-boundary.patch
patch -p1 -u -f < ../patches/qemu-0.9.0-i386-FORCE_RET.patch
patch -p1 -u < ../patches/qemu-0.9.0-osx-intel-port.patch

patch -p1 -u < ../patches/qemu-0.8.0-osx-bugfix.patch

# arm patches
patch -p1 -u < ../patches/qemu-0.9.0-arm-shift.patch

# x49gp patches
patch -p1 -u < ../patches/qemu-0.9.0-sparc-compile-flags.patch
patch -p1 -u < ../patches/qemu-0.9.0-sparc-load-store-le.patch
patch -p1 -u < ../patches/qemu-0.9.0-sparc-clobber.patch
patch -p1 -u < ../patches/qemu-0.9.0-sparc-register.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-arm-dump-state.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-arm-mmu.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-arm-semihosting.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-debug-unassigned.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-phys_ram_dirty.patch
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-block.patch

# only build libqemu.a
patch -p1 -u < ../patches/qemu-0.9.0-x49gp-build-libqemu.patch


# configure
if [ "`uname -m`" = "sparc64" ]; then
	STUB=sparc32
fi

if [ "`uname -s`" = "Darwin" ]; then
	OPTIONS="--disable-gcc-check"
fi

OPTIONS="${OPTIONS} --disable-gfx-check"

${STUB} ./configure ${OPTIONS} --target-list=arm-softmmu

cd ..
