#!/usr/bin/env bash
: '
python3 pcap_parser_and_plotter.py -d fresh50/rr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/rr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/wrr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/wrr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/strict/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/strict/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_strict/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_strict/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_rr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_rr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_wrr/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_wrr/ -p 100 &

python3 pcap_parser_and_plotter.py -d fresh50/inorder_only/ -p 50 &
python3 pcap_parser_and_plotter.py -d fresh100/inorder_only/ -p 100 &
'

#mechanism=$(echo $1)
#queues=$(echo $2)
#trafficPattern=$(echo $3)
#for queues in 64;do
    #for trafficPattern in 6 7 8 9;do
        #for mechanism in 0 1 2 3 5;do
    #        for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 775000 800000;do
                #python3 pcap_parser_new.py -i $mechanism -p $queues -t $trafficPattern -l $load &
         #       python3 collect_load_latency.py -i $mechanism -p $queues -t $trafficPattern -l $load &
            #done
        #done
    #done
#done
for queues in 64;do
    for trafficPattern in 6 7 8 9;do
        for mechanism in 1 2 3 5;do
            python3 collect_load_latency.py -i $mechanism -t $trafficPattern -q $queues &
        done
    done
done

: '
for queues in 1;do
    for trafficPattern in 7;do
        for mechanism in 0;do
            python3 collect_load_latency.py -i $mechanism -t $trafficPattern -q $queues &
        done
    done
done
'
#./parse_all.sh 0 1 7
#python3 combineCSVs.py -i 0 -p 1 -t 7
#./parse_all.sh 1 64 6
#python3 combineCSVs.py -i 1 -p 64 -t 6
#./parse_all.sh 2 64 6
#python3 combineCSVs.py -i 2 -p 64 -t 6
#./parse_all.sh 3 64 6
#python3 combineCSVs.py -i 3 -p 64 -t 6
#./parse_all.sh 5 64 6
#python3 combineCSVs.py -i 5 -p 64 -t 6
