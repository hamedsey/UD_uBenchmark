#!/usr/bin/env bash
#make
#i=1
dirName=$(echo $1)
serverQpn=$(echo $2)
#queues=$(echo $3)
#serverIPAddr=$(echo $4)
#ib_devname=$(echo $5)
#gid=$(echo $6)

./disable_icrc.sh
#ping 192.168.1.5 -c 3
#sleep 20
mkdir $dirName

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
: '
for queues in 64;do #1 8 16 32 64 128 256;do
    mkdir $dirName"/"$queues
    for dist in 7 9;do
        mkdir $dirName"/"$queues"/"$dist
        for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 800000;do
            mkdir $dirName"/"$queues"/"$dist"/"$load
            #sudo tcpdump -i enp24s0 src 192.168.1.5 and dst port 4791 -w "fresh"$queues"/"$dirName"/"$load.pcap &
            #if doing sq/one priority change n to one otherwise $queues 
            ./UD_Client -w 1 -t 1 -d $dirName"/"$queues"/"$dist"/"$load -v mlx5_0 -g 3 -m $dist -s 192.168.1.5 -l $load -q $serverQpn -n $queues >> $dirName"/"$queues"/"$dist"/"$load"/log.txt"
            #./UD_Client -w 128 -t 1 -d lat -v mlx5_0 -g 3 -m 5 -s 192.168.1.5 -c 1 -n $queues -q $serverQpn -l $load >> "fresh"$queues"/"$dirName"/"runlog.txt
            
            sleep 15
            ((serverQpn+=512))
            #serverQpn = $((serverQpn + 512))
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

: '
'
for queues in 64;do #8 16 32 64 128 256;do
    mkdir $dirName"/"$queues
    for dist in 5 6;do
        echo $dist
        mkdir $dirName"/"$queues"/"$dist
        for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 775000 800000;do
            echo $load
            mkdir $dirName"/"$queues"/"$dist"/"$load
            #sudo tcpdump -i enp24s0 -s 100 src 192.168.1.5 and dst port 4791 -w $dirName"/"$queues"/"$dist"/"$load"/"$load.pcap &
            #if doing sq/one priority change n to one otherwise $queues 
            ./UD_Client -w 16384 -t 1 -d $dirName"/"$queues"/"$dist"/"$load -v mlx5_0 -g 3 -m $dist -s 192.168.1.5 -l $load -q $serverQpn -n $queues >> $dirName"/"$queues"/"$dist"/"$load"/log.txt"
            #./UD_Client -w 1 -t 1 -d $dirName"/"$queues"/"$dist"/"$load -v mlx5_0 -g 3 -m $dist -s 192.168.1.5 -l $load -q $serverQpn -n $queues >> $dirName"/"$queues"/"$dist"/"$load"/log.txt"
            #./UD_Client -w 128 -t 1 -d lat -v mlx5_0 -g 3 -m 5 -s 192.168.1.5 -c 1 -n $queues -q $serverQpn -l $load >> "fresh"$queues"/"$dirName"/"runlog.txt
            
            drop=$(tail -n 1 $dirName"/"$queues"/"$dist"/"$load"/log.txt")
            echo $drop
            if [[ drop == 1 ]];then
                echo "too many drops!"
            fi

            sleep 5
            ((serverQpn+=512))
            #sudo kill -15 $(pgrep "tcpdump")
            #mv $dirName"/"$queues"/"$dist"/"$load"/"$load.pcap $dirName"/"$queues"/"$dist"/"$load"/"$rps.pcap 

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
: '
for queues in 64;do #32 64 128 256;do
    mkdir $dirName"/"$queues
    for dist in 8;do
        mkdir $dirName"/"$queues"/"$dist
        for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 800000;do
            mkdir $dirName"/"$queues"/"$dist"/"$load
            #sudo tcpdump -i enp24s0 src 192.168.1.5 and dst port 4791 -w "fresh"$queues"/"$dirName"/"$load.pcap &
            #if doing sq/one priority change n to one otherwise $queues 
            ./UD_Client -w 1 -t 1 -d $dirName"/"$queues"/"$dist"/"$load -v mlx5_0 -g 3 -m $dist -s 192.168.1.5 -l $load -q $serverQpn -n $queues >> $dirName"/"$queues"/"$dist"/"$load"/log.txt"
            #./UD_Client -w 128 -t 1 -d lat -v mlx5_0 -g 3 -m 5 -s 192.168.1.5 -c 1 -n $queues -q $serverQpn -l $load >> "fresh"$queues"/"$dirName"/"runlog.txt
            
            sleep 15
            ((serverQpn+=512))
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