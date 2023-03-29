#!/usr/bin/env python

import dpkt
import socket
import time

import numpy as np
from statistics import mean
import argparse
import csv
import os, glob
import sys


# Initialize parser
parser = argparse.ArgumentParser()

# Adding optional argument
#parser.add_argument("-f", "--filePath", help = "path to pcap file")
parser.add_argument("-d", "--directory", help = "input and output directory")

# Read arguments from command line
args = parser.parse_args()
input_dir = args.directory
if input_dir[-1] == "/":
    input_dir = input_dir[:-1]

print(input_dir)

#Create results and graphs folders
if not  os.path.exists(input_dir+'/result'):
	os.makedirs(input_dir+'/result')

folder_path_list = glob.glob(input_dir+"*")
if len(folder_path_list) == 0:
    sys.exit('Input directory incorrect')

#path2File = args.filePath

clk_period = 4.6
skip_the_first = 10000

for folder_path in folder_path_list:
    print(folder_path)
    file_list = glob.glob(folder_path+"/*.pcap") 
    file_list = sorted(file_list,key = os.path.getmtime)
    results = [[]]
    #print("File Name , Average , p50 , p90 , p95 , p99 , p999")

    for filename in file_list:
        print(filename)
        latencies = []

        f = open(filename,'rb')
        pcap = dpkt.pcap.Reader(f)
        i = 0
        longLatencyCount = 0

        for ts, buf in pcap:
            i = i+1
            if i < skip_the_first: continue
            eth = dpkt.ethernet.Ethernet(buf)
            ip = eth.data
            udp = ip.data
            payload = udp.data.hex()
            #print(payload[45:60])
            egressTimestamp = int(payload[44:52], 16)
            ingressTimestamp = int(payload[52:60], 16)
            priorityID = int(payload[60:62], 16)
            #print(priorityID)
            if priorityID == 0:
                if egressTimestamp < ingressTimestamp: latency = 4294967295 + egressTimestamp - ingressTimestamp
                else: latency = egressTimestamp - ingressTimestamp
                if latency > 1500 and i > skip_the_first: longLatencyCount = longLatencyCount + 1
                #if i < 1000: print(latency)
                #print(latency)

                latencies.append(latency)
                #egressTimestamp =(payload[52])|(payload[51]<<4)|(payload[50]<<8)|(payload[49]<<12)|(payload[48]<<16)|(payload[47]<<20)|(payload[46]<<24)|(payload[45]<<28)

        avg = mean(latencies)
        #p50 = np.percentile(latencies, 50)
        #p90 = np.percentile(latencies, 90)
        #p95 = np.percentile(latencies, 95)
        p99 = np.percentile(latencies, 99)
        p999 = np.percentile(latencies, 99.9)
        result = [filename, avg*clk_period, p50*clk_period, p90*clk_period, p95*clk_period, p99*clk_period, p999*clk_period]
        #print(result)
        results.append(result)

    dir = filename.split('/')

    with open(dir[0]+'/'+dir[1]+'/result/'+dir[1]+'.csv', 'w') as f:
        # create the csv writer
        writer = csv.writer(f)

        # write a row to the csv file
        writer.writerow(["File Name", "Average", "p50","p90", "p95", "p99", "p999"])
        writer.writerows(results)

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
