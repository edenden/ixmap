#! /bin/sh

num_ports=`lspci -v | grep ixmap | wc -l`
num_cores=`nproc`

exec ixmap -n $num_ports -t $num_cores
