#!/bin/sh

if [ -f "Makefile" ]
then 
    make maintainer-clean
fi
../configure CFLAGS="-g -O0" CXXFLAGS="-g -O0" && \
make -j`grep -c ^processor /proc/cpuinfo` && \
echo "Done!"

