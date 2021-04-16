#!/bin/sh

if [ `uname` != "Darwin" ]; then
  export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib
fi

isDaemon=false

while [ $# != 0 ]
do
  if [ "$1" == "-d" ]; then
    isDaemon=true
  fi
  shift
done

cd ..

echo "building..."
if [ ! -d "./build" ]; then
  mkdir build
fi

cd build

if ${isDaemon}; then
  cmake -DDAEMON=On .. && make
else
  cmake -DDAEMON=Off .. && make
fi

