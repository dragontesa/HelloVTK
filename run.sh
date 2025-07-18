#!/usr/bin/sh
f_help()
{
  echo "Usage is one of below"
  echo "[Target Dir] [.vtk File] or"
  echo "[Target Dir] [Executable Name] [.vtk File]"
  echo "e.g) ./run.sh VtkReader Your.vtk"
  echo "e.g) ./run.sh VtkReader MyReadPolyDataMapper Your.vtk"
}

if [ $# -le 1 ]; then
  f_help
  exit
elif [ -z $1 ];then
  f_help
  exit
elif [ -z $2 ];then
  f_help
  exit;
fi

Example=$1
run_target=$1
vtk_file=$2

if [ $# -gt 2 ]; then
  run_target=$2
  vtk_file=$3
  echo "run_target> $run_target"
  echo "file> $vtk_file"
fi
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/vtk/9.4.1/lib64
./build/${Example}/${run_target} $vtk_file