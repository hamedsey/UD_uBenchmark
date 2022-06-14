#!/usr/bin/env bash
make
# source /opt/Xilinx/Vivado/2016.2/settings64.sh
# sudo modprobe mlx_accel_tools
# sudo mst start --with_fpga
# sudo /./opt/Xilinx/Vivado/2016.2/bin/hw_serverpv
# sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga_rdma query
# sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga_rdma load
for i in $(seq 1 40);
do
    echo $i
    date
    ./UD_Server &
    sleep 25
    date
    kill -9 $(ps aux | grep -i "./UD_Server" | cut -d ' ' -f 3)
done