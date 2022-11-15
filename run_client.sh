#!/usr/bin/env bash
#make
#i=1
dirName=$(echo $1)
serverQpn=$(echo $2)
servTimeDist=$(echo $3)
serverIPAddr=$(echo $4)
ib_devname=$(echo $5)
gid=$(echo $6)
ratios=$(echo $7)
percents=$(echo $8)
server_cores=$(echo $9)

#rps=0
#rps_prev=0
#break_flag=0

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

#if [ ! -d $dirName ];
#then
#    mkdir $dirName
#fi

if [ $servTimeDist == 4 ];then
    for ratio in $ratios; do #15 25 50; do # 2 5 10 25 50 100;do
        for percent in $percents; do #30 ;do #1 5 10 25 50;do
            echo "hamed is here 1"
            mkdir $dirName"_r"$ratio"_p"$percent
            for windowSize in 1 2 4 8 16; do
                for threadNum in 1; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            for windowSize in 8 12; do
                for threadNum in 2; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            for windowSize in 8 12 16; do
                for threadNum in 4; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            
            for windowSize in 8 9 10 12 16; do
                for threadNum in 8; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            for windowSize in 12 16; do
                for threadNum in 12; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            for windowSize in 16; do
                for threadNum in 16 20 24 28; do
                    for iter in 1; do
                        # echo $i
                        # date
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            #rps=0
            #rps_prev=0
            #for windowSize in 1 2;do #8 16 32 40 48; do
            #    for threadNum in 1; do
                    # echo $i
                    # date
            #        for iter in 1; do
            #            ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
            #        done
            #    done
            #done
            #for windowSize in 1; do
            #    for threadNum in 4 8 12 16 20 23 ; do
                    # echo $i
                    # date
            #        for iter in 1; do
            #            ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
            #        done
            #    done
            #done
            : '
            for windowSize in 2; do
                for threadNum in 10 ; do
                    # echo $i
                    # date
                    for iter in 1; do
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
                    done
                done
            done
            '
            : '
            for windowSize in 2 3 4 5 6 7 8 12 16 20 24;do #28 32 36 40 44 48;do
                for threadNum in 16;do
                        # echo $i
                        # date
                    for iter in 1; do
                        ./UD_Client -w $windowSize -t $threadNum -d $dirName"_r"$ratio"_p"$percent -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -r $ratio -p $percent >> runlog.txt
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
        done
    done
else
    mkdir $dirName
    echo "hamed is here 2"
    for windowSize in 4 8 16; do
        for threadNum in 1; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done
    for windowSize in 8 12; do
        for threadNum in 2; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done
    for windowSize in 8 12 16; do
        for threadNum in 4; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done
    
    for windowSize in 8 9 10 12 16; do
        for threadNum in 8; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done
    for windowSize in 12 16; do
        for threadNum in 12; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done
    for windowSize in 16; do
        for threadNum in 16 20 24 28; do
            for iter in 1; do
                # echo $i
                # date
                ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c $server_cores >> runlog.txt
            done
        done
    done

    
    #for windowSize in 5;do #8 16 32 40 48; do
    #    for threadNum in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17; do
    #        for iter in 1; do
                # echo $i
                # date
    #            ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c 16 >> runlog.txt
    #        done
    #    done
    #done
    
    #for windowSize in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
    #    for threadNum in 20; do
    #        for iter in 1; do
                # echo $i
                # date
    #            ./UD_Client -w $windowSize -t $threadNum -d $dirName -v $ib_devname -g $gid -q $serverQpn -m $servTimeDist -s $serverIPAddr -c 16 >> runlog.txt
    #        done
    #    done
    #done

fi
