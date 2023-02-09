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
parser.add_argument("-f", "--filePath", help = "path to pcap file")

# Read arguments from command line
args = parser.parse_args()
path2File = args.filePath

'''
p50_list  = []
p95_list  = []
p99_list  = []
p999_list = []
'''
latencies = []

clk_period = 4.6

f = open(path2File,'rb')
pcap = dpkt.pcap.Reader(f)
i = 0
longLatencyCount = 0

for ts, buf in pcap:
    i = i+1
    if i < 100000: continue
    eth = dpkt.ethernet.Ethernet(buf)
    ip = eth.data
    udp = ip.data
    payload = udp.data.hex()
    #print(payload[45:60])
    egressTimestamp = int(payload[45:52], 16)
    ingressTimestamp = int(payload[53:60], 16)
    if egressTimestamp < ingressTimestamp: latency = 4294967295 + egressTimestamp - ingressTimestamp
    else: latency = egressTimestamp - ingressTimestamp
    if latency > 1500 and i > 100000: longLatencyCount = longLatencyCount + 1
    #if i < 1000: print(latency)
    #print(latency)

    latencies.append(latency)
    #egressTimestamp =(payload[52])|(payload[51]<<4)|(payload[50]<<8)|(payload[49]<<12)|(payload[48]<<16)|(payload[47]<<20)|(payload[46]<<24)|(payload[45]<<28)

p50 = np.percentile(latencies, 50)
p90 = np.percentile(latencies, 90)
p95 = np.percentile(latencies, 95)
p99 = np.percentile(latencies, 99)
p999 = np.percentile(latencies, 99.9)

print("longLatencyCount = ", longLatencyCount)
print("avg = ", mean(latencies)*clk_period)
print("i = ", i)
print("Printing Percentiles")
print("p50 = ", p50*clk_period)
print("p90 = ",p90*clk_period)
print("p95 = ",p95*clk_period)
print("p99 = ",p99*clk_period)
print("p999 = ",p999*clk_period)
