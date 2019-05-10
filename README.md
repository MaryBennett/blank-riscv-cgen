# blank-riscv-cgen
This is repository contains the files I changed in Binutils to generate a RISC-V CGEN port as part of my third-year project.

To generate the port, run:

git clone git clone https://github.com/embecosm/riscv-toolchain.git

./clone-all.sh

./build-targets.sh

git clone https://github.com/embecosm/cgen.git

cp -nR ../cgen/*

Then add the files:
 * ~/binutils/gas/config/tc-riscv.c
 * ~/binutils/gas/config/tc-riscv.h
 * ~/binutils/cpu/riscv.cpu
 * ~/binutils/cpu/riscv.opc
 * ~/binutils/opcodes/configure.ac
 * ~/binutils/opcodes/Makefile.am
 * ~/binutils/gas/configure.ac

Remember to regenerate the Configure file for opcodes/ and gas/ using the command:

autoreconf -vfi
