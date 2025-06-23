#!/usr/bin/sh
if [ "$#" -ne 2 ]; then
  echo "Target VTK is not specified"
  exit
elif [ -z $1 ];then
  echo "Target VTK is not specified"
  exit
elif [ -z $2 ];then
  echo "VTK file to read is not specified"
  exit;
fi

vtk_target=$1
vtk_file=$2

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/vtk/9.4.1/lib64
./build/${vtk_target}/${vtk_target} $vtk_file