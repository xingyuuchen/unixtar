#!/bin/sh

# Before you run this script, make sure you have installed protobuf.

cd ..

project_dir=$(pwd)

if [ ! -d "/autogen" ]; then
  mkdir -p autogen
fi 

cd protos

for file in `ls`
do

  if [ ${file##*.} == "proto" ]; then
    echo "processing" ${file}" ..."

    protoc -I=$(pwd) --cpp_out=${project_dir}/autogen \
      $(pwd)/${file}
  fi

done

