#!/usr/bin/env python

import dpkt
import socket
import time

import numpy as np
import matplotlib.pyplot as plt
from statistics import mean
import argparse
import csv
import os, glob
import sys

# Initialize parser
parser = argparse.ArgumentParser()

# Adding optional argument
parser.add_argument("-i", "--index", help = "index corresponding to notification mechanism")
parser.add_argument("-p", "--priorities", help = "number of priorities")
parser.add_argument("-t", "--trafficpattern", help = "traffic pattern ID: FB(5), PC(6), SQ(7), NC(8), EXP(9)")
parser.add_argument("-l", "--generatedload", help = "load generated by client")


#python3 pcap_parser_new.py -i 2 -p 64 -t 6
#python3 pcap_parser_new.py -i 3 -p 64 -t 6
#python3 pcap_parser_new.py -i 5 -p 64 -t 6

# Read arguments from command line
args = parser.parse_args()
index = int(args.index)
priorities = int(args.priorities)
trafficpattern = int(args.trafficpattern)
generatedload = int(args.generatedload)

mechanisms = ["IDEAL", "SW_again", "INORDER_again", "LZCNT", "AVX", "BLINDPOLL"]

'''
#Create results and graphs folders
if not os.path.exists('result'):
	os.makedirs('result')
	for mechanism in mechanisms:
		os.makedirs('result/'+mechanism)

input_dir = args.directory
if input_dir[-1] == "/":
    input_dir = input_dir[:-1]

print(input_dir)
path = input_dir.split("/")

#Create results and graphs folders
if not  os.path.exists(input_dir+'/result'):
	os.makedirs(input_dir+'/result')

if not  os.path.exists(input_dir+'/graph'):
	os.makedirs(input_dir+'/graph')
'''

clk_period = 4.6
queues = 64
#trafficPatternID = [5,6,7,8,9]
trafficpatterns = ["NULL", "NULL", "NULL", "NULL", "NULL", "FB", "PC", "SQ", "NC", "EXP"]
#generatedLoad = ["100000","200000","300000","400000","500000","600000","700000","800000","900000","1000000"]
#generatedLoad = ["500000","525000","550000","575000","600000","625000","650000", "675000", "700000", "725000", "750000", "800000"]

mechanism = mechanisms[index]

#INORDER/64/6/
#folder_path_list = glob.glob(mechanism+"/"+str(priorities)+"/"+str(trafficpattern)+"/*")
#if len(folder_path_list) == 0:
#    sys.exit('Input directory incorrect')

#sorts list of folders in order in which they were created
#folder_path_list.sort(key=lambda x: os.path.getmtime(x))
#path2File = args.filePath

folder_path = mechanism+"/"+str(priorities)+"/"+str(trafficpattern)+"/"+str(generatedload)
clk_period = 4.6
skip_the_first = 10000

# Creates a list containing 50 lists, each of 0 items, all set to 0
w, h = 0, priorities

#load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]
p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

p95Latencies = []
p99Latencies = []
p999Latencies= []

#for folder_path in folder_path_list:
print(folder_path)
#file_list = [folder_path+"/100000.pcap",folder_path+"/200000.pcap",folder_path+"/300000.pcap",folder_path+"/400000.pcap",folder_path+"/500000.pcap",
#             folder_path+"/600000.pcap",folder_path+"/700000.pcap",folder_path+"/800000.pcap",folder_path+"/900000.pcap",folder_path+"/1000000.pcap"]
file_list = glob.glob(folder_path+"/*.pcap") 
#file_list = sorted(file_list,key = os.path.getmtime)
results = [[]]
#print("File Name , Average , p50 , p90 , p95 , p99 , p999")

for filename in file_list:
    prioLatencies = [[0 for x in range(w)] for y in range(h)] 
    print(filename)
    latencies = []
    numCompletedPrio = [0]*priorities
    f = open(filename,'rb')
    pcap = dpkt.pcap.Reader(f)
    i = 0
    #longLatencyCount = 0
    rest, ext = filename.split(".")
    mech, qp, tp, genload, load = rest.split("/")
    #qs, rest = rest.split("/")
    #tp, rest = rest.split("/")
    #genload, load = rest.split("/")

    for ts, buf in pcap:
        #print(i)
        i = i+1
        if i < skip_the_first: continue
        eth = dpkt.ethernet.Ethernet(buf)
        ip = eth.data
        udp = ip.data
        payload = udp.data.hex()
        #print(payload[44:52])
        #print(payload[52:60])
        #print(payload[60:62])
        egressTimestamp = int(payload[44:52], 16)
        ingressTimestamp = int(payload[52:60], 16)
        priorityID = int(payload[60:62], 16)
        
        numCompletedPrio[priorityID] = numCompletedPrio[priorityID]+1

        #print(priorityID)
        #if priorityID == 0:
        if egressTimestamp < ingressTimestamp: latency = (4294967295 + egressTimestamp - ingressTimestamp)*clk_period
        else: latency = (egressTimestamp - ingressTimestamp)*clk_period
        #if latency > 1500 and i > skip_the_first: longLatencyCount = longLatencyCount + 1
        #if i < 1000: print(latency)
        #print(latency)

        latencies.append(latency)
        prioLatencies[priorityID].append(latency)
        #egressTimestamp =(payload[52])|(payload[51]<<4)|(payload[50]<<8)|(payload[49]<<12)|(payload[48]<<16)|(payload[47]<<20)|(payload[46]<<24)|(payload[45]<<28)

    avg = mean(latencies)
    #p50 = np.percentile(latencies, 50)
    #p90 = np.percentile(latencies, 90)
    #p95 = np.percentile(latencies, 95)
    tempP95 = np.percentile(latencies, 95)
    p95Latencies.append(tempP95)
    tempP99 = np.percentile(latencies, 99)
    p99Latencies.append(tempP99)
    tempP999 = np.percentile(latencies, 99.9)
    p999Latencies.append(tempP999)

    result = [filename, load, avg*clk_period, tempP95*clk_period, tempP99*clk_period, tempP999*clk_period]

    for x in range(priorities):
        #print(x)
        #print(prioLatencies[x])
        tempP95 = np.percentile(prioLatencies[x], 95)
        tempP99 = np.percentile(prioLatencies[x], 99)
        tempP999 = np.percentile(prioLatencies[x], 99.9)
        #p99PrioLatencies[x].append(tempP99)
        #p999PrioLatencies[x].append(tempP999)
        result.append(tempP95*clk_period)
        result.append(tempP99*clk_period)
        result.append(tempP999*clk_period)

    for x in range(priorities):
        result.append(numCompletedPrio[x])

    #print(result)
    results.append(result)


with open(mechanism+'/'+str(priorities)+'/'+str(trafficpattern)+'/'+str(generatedload)+'/result.csv', 'w') as f:
    # create the csv writer
    writer = csv.writer(f)

    # write a row to the csv file
    writer.writerow(["File Name", "Load" ,"Average", 
                                                "p95_all", "p99_all", "p999_all", "p95_P0", "p99_P0","p999_P0", "p95_P1", "p99_P1","p999_P1","p95_P2","p99_P2","p999_P2","p95_P3","p99_P3","p999_P3","p95_P4","p99_P4","p999_P4","p95_P5","p99_P5","p999_P5"
                                                #"p99_P6","p999_P6","p99_P7","p999_P7","p99_P8","p999_P8","p99_P9","p999_P9","p99_P10","p999_P10","p99_P11","p999_P11",
                                                #"p99_P12","p999_P12","p99_P13","p999_P13","p99_P14","p999_P14","p99_P15","p999_P15","p99_P16","p999_P16","p99_P17","p999_P17",
                                                #"p99_P18","p999_P18","p99_P19","p999_P19","p99_P20","p999_P20","p99_P21","p999_P21","p99_P22","p999_P22","p99_P23","p999_P23",
                                                #"p99_P24","p999_P24","p99_P25","p999_P25","p99_P26","p999_P26","p99_P27","p999_P27","p99_P28","p999_P28","p99_P29","p999_P29",
                                                #"p99_P30","p999_P30","p99_P31","p999_P31","p99_P32","p999_P32","p99_P33","p999_P33","p99_P34","p999_P34","p99_P35","p999_P35",
                                                #"p99_P36","p999_P36","p99_P37","p999_P37","p99_P38","p999_P38","p99_P39","p999_P39","p99_P40","p999_P40","p99_P41","p999_P41",
                                                #"p99_P42","p999_P42","p99_P43","p999_P43","p99_P44","p999_P44","p99_P45","p999_P45","p99_P46","p999_P46","p99_P47","p999_P47",
                                                #"p99_P48","p999_P48","p99_P49","p999_P49"
                                                ])
    writer.writerows(results)

'''
figure, ((ax)) = plt.subplots(2, 1)
ax[0].plot(load_list, p99Latencies, '-' , marker='x', label="all")
ax[1].plot(load_list, p999Latencies, '-' , marker='x', label="all")

for x in range(priorities):
    if(x % 10 == 0):
        ax[0].plot(load_list, p99PrioLatencies[x], label=x)
        ax[1].plot(load_list, p999PrioLatencies[x], label=x)

ax[0].legend()
ax[1].legend()

ax[0].set_ylabel('p99 latency (ns)', fontsize=20)
ax[0].set_ylabel('p999 latency (ns)', fontsize=20)
ax[1].set_xlabel('Load (RPS)', fontsize=20) 
plt.savefig(input_dir+'/graph/'+path[1]+'_priority_plot.pdf',bbox_inches='tight')
'''

'''
print("longLatencyCount = ", longLatencyCount)
print("avg = ", mean(latencies)*clk_period)
print("i = ", i)
print("Printing Percentiles")
print("p50 = ", p50*clk_period)
print("p90 = ",p90*clk_period)
print("p95 = ",p95*clk_period)
print("p99 = ",p99*clk_period)
print("p999 = ",p999*clk_period)
'''
