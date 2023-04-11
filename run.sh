#!/usr/bin/env bash
#make
#i=1
dirName=$(echo $1)
serverQpn=$(echo $2)
queues=$(echo $3)
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


mkdir "fresh"$queues"/"$dirName
: '
'
for load in 50000 100000 200000 300000 400000 500000 600000 700000 800000 900000 1000000;do
    sudo tcpdump -i enp24s0 src 192.168.1.5 and dst port 4791 -w "fresh"$queues"/"$dirName"/"$load.pcap &
    #if doing sq change n to one otherwise $queues 

    ./UD_Client -w 128 -t 1 -d lat -v mlx5_0 -g 3 -m 5 -s 192.168.1.5 -c 1 -n 1 -q $serverQpn -l $load >> "fresh"$queues"/"$dirName"/"runlog.txt
    #sleep 10
    sudo kill -15 $(pgrep "tcpdump")

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