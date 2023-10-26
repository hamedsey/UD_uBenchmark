#!/usr/bin/env bash
#make
#i=1
#dirName=$(echo $1)
#serverQpn=$(echo $2)
#queues=$(echo $3)
#serverIPAddr=$(echo $4)
#ib_devname=$(echo $5)
#gid=$(echo $6)

: '
if [[ "$servTimeDist" == "FIXED" ]];
then
    servTimeDist=0
elif [[ "$servTimeDist" == "NORMAL" ]];
then
    servTimeDist=1
elif [[ "$servTimeDist" == "UNIFORM" ]];
then
    servTimeDist=2
elif [[ "$servTimeDist" == "EXPONENTIAL" ]];
then
    servTimeDist=3
elif [[ "$servTimeDist" == "BIMODAL" ]];
then
    servTimeDist=4
else
    echo "Unsupported request distribution mode! Pick one from: FIXED; NORMAL; UNIFORM; EXPONENTIAL."
fi
'
#if [ ! -d $dirName ];
#then
#    mkdir $dirName
#fi

#the mechanism is the dirName (i.e. LZCNT, AVX, SW, IDEAL SRQ)
#mkdir $dirName
: '
sudo /etc/init.d/openibd restart
sudo modprobe mlx_accel_tools
sudo mst start --with_fpga
sudo /./opt/Xilinx/Vivado/2016.2/bin/hw_serverpv
sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga_rdma load --factory
sudo mlx_fpga -d /dev/mst/mt4117_pciconf0_fpga_rdma query
sudo mcra /dev/mst/mt4117_pciconf0 0x5363c.12:1 0
sudo mcra /dev/mst/mt4117_pciconf0 0x5367c.12:1 0
sudo mcra /dev/mst/mt4117_pciconf0 0x53628.3:1 0
sudo mcra /dev/mst/mt4117_pciconf0 0x53668.3:1 0
sudo iptables -F; sudo iptables -t mangle -F
sudo iptables -D INPUT -m conntrack --ctstate INVALID -j DROP
sudo ethtool --set-priv-flags enp24s0 sniffer off
/root/disable-mellanox-shell-credits.sh
/home/hseyedro3/disable-mellanox-shell-credits.sh
sleep 20
'

echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu5/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu6/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu7/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu8/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu9/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu10/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu11/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu12/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu13/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu14/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu15/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu16/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu17/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu18/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu19/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu20/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu21/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu22/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu23/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu24/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu25/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu26/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu27/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu28/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu29/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu30/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu31/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu32/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu33/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu34/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu35/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu36/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu37/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu38/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu39/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu40/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu41/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu42/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu43/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu44/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu45/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu46/cpufreq/scaling_governor
echo performance > /sys/devices/system/cpu/cpu47/cpufreq/scaling_governor

: '
for queues in 64;do #1 8 16 32 64 128 256;do
    #mkdir $dirName"/"$queues
    for dist in 6 7 9;do #5 7 9 6 8;do
        #mkdir $dirName"/"$queues"/"$dist
        for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 775000 800000;do
            #sudo tcpdump -i enp24s0 src 192.168.1.5 and dst port 4791 -w "fresh"$queues"/"$dirName"/"$load.pcap &
            #if doing sq/one priority change n to one otherwise $queues 
            #./UD_Client -w 1 -t 1 -d $dirName"/"$queues"/"$dist -v mlx5_0 -g 3 -m $dist -s 192.168.1.5 -l $load -q $serverQpn -n $queues
            ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 1 -q $queues -n $queues -b 100
            

            #sudo kill -15 $(pgrep "tcpdump")

            #./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
            #if [[ rps == 0 ]];then
            #	rps=$(tail -n 1 runlog.txt)
            #else
            #    rps_prev=$rps
            #    rps=$(tail -n 1 runlog.txt)
                #if [[ rps -lt rps_prev*97/100 ]];then
                    #break 2
                #fi
            #fi
        done
    done
done
'

#for priorities in 64;do #1 8 16 32 64 128 256;do
#for queues in 64;do #1 8 16 32 64 128 256;do
for queues in 1020;do #8 16 32 64 128 256;do
    #mkdir $dirName"/"$queues
    for dist in 5 6 9 10;do #5 7 9 6 8;do
        #mkdir $dirName"/"$queues"/"$dist
        #for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 800000;do

        #for load in 1000000 2000000 3000000 4000000 6000000 8000000 9000000 10000000 11000000 12000000 13000000 14000000 15000000 16000000 17000000 18000000 19000000 20000000;do #22000000 24000000 26000000 28000000;do #scaleout

        #./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q $queues -n $queues -b 100
        #./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 1 -q $queues -n $queues -b 100

        #Global SQ
        #for load in 50000 100000 150000 200000 250000 300000 350000 400000 450000 500000 550000 600000 650000 700000;do  #for global prio (SQ and MQ)
        #    ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 1 -n 48 -b 100
        #Global 12Q / Local 12Q
        #for load in 50000 100000 150000 200000 250000 300000 350000 400000 450000 500000 550000 600000 650000 700000;do  #for global prio (SQ and MQ)
        for load in 1000000 2000000 3000000 4000000 6000000 8000000 9000000 10000000 11000000 12000000 13000000 14000000 15000000 16000000 17000000 18000000 19000000 20000000;do #22000000 24000000 26000000 28000000;do #scaleout
            ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 12 -n 48 -b 100

        #SUPP 48Q / Local 48Q
        #for load in 1000000 2000000 3000000 4000000 6000000 8000000 9000000 10000000 11000000 12000000 13000000 14000000 15000000 16000000 17000000 18000000 19000000 20000000;do #22000000 24000000 26000000 28000000;do #scaleout
        #for load in 100000 200000 300000 400000 500000 600000 700000 800000 900000 1000000 1100000 1200000 1300000;do #scale up SUPP
        #    ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 48 -n 48 -b 100
        #Group 12T/6Q
        #for load in 300000 600000 900000 1200000 1500000 1800000 1900000 2000000 2100000 2200000 2300000 2400000 2500000;do  #for 12 queues, groups of 2
        #    ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 6 -n 48 -b 100
        #Group 12T/3Q
        #for load in 200000 400000 600000 800000 1000000 1100000 1200000 1300000 1400000 1500000;do  #for 12 queues, groups of 4
        #    ./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 3 -n 48 -b 100

        #No priority - IDEAL
        #for load in 1000000 2000000 3000000 4000000 6000000 8000000 9000000 10000000 11000000 12000000 13000000 14000000 15000000 16000000 17000000 18000000 19000000 20000000;do #22000000 24000000 26000000 28000000;do #scaleout
            #./UD_Server -s 192.168.1.20 -g 4 -v mlx5_0 -t 12 -q 12 -n 48 -b 100


            #sudo kill -15 $(pgrep "tcpdump")

            #./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
            #if [[ rps == 0 ]];then
            #	rps=$(tail -n 1 runlog.txt)
            #else
            #    rps_prev=$rps
            #    rps=$(tail -n 1 runlog.txt)
                #if [[ rps -lt rps_prev*97/100 ]];then
                    #break 2
                #fi
            #fi
        done
    done
done
#done


