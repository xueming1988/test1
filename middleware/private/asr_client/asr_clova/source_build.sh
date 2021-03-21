#!/bin/bash

export ROOT_DIR=${PWD}
# Please change this path to your toolchain's path
export TOOLCHAIN=${ROOT_DIR}/../../../platform/amlogic/a113/toolchain/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf
export PATH=${TOOLCHAIN}/bin:${PATH}

cd asr_clova; make clean; make; make install