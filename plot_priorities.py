import numpy as np
import matplotlib.pyplot as plt
from statistics import mean
import argparse
from csv import reader
import os, glob
import sys
from scipy.optimize import curve_fit
from numpy import arange
import math
from scipy import interpolate
from matplotlib.pyplot import text

# Initialize parser
parser = argparse.ArgumentParser()

# Adding optional argument
#parser.add_argument("-f", "--filePath", help = "path to pcap file")
parser.add_argument("-d", "--directory", help = "path to csv file")

# Read arguments from command line
args = parser.parse_args()
input_dir = args.directory
if input_dir[-1] == "/":
    input_dir = input_dir[:-1]

path = input_dir.split("/")

queues = 64
trafficpatterns = ["PC", "SQ", "NC", "EXP"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]


priorities = 50
# Creates a list containing 50 lists, each of 0 items, all set to 0
w, h = 0, priorities

load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]
p95PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

p95Latencies = []
p99Latencies = []
p999Latencies = []

#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

#file_list = [folder+"d-FCFS(r)-result.csv", folder+"d-FCFS(rr)-result.csv", folder+"JSQ-result.csv", folder+"JLQ-result.csv"]
file_list = glob.glob(input_dir+"/result/*.csv") 
#color_index = 0
for result_file in file_list:

    print(result_file)
    load_list = []

    with open(result_file, 'r') as read_obj:
        # pass the file object to reader() to get the reader object
        csv_reader = reader(read_obj)
        header = next(csv_reader)
        if header != None:
            for row in csv_reader:
                if len(row) != 0:
                    if row[1] != '':
                    #if float(row[5]) <= 200:
                        load_list.append(float(row[1])) # Convert to MRPS
                        p95Latencies.append(float(row[3]))
                        p99Latencies.append(float(row[4]))
                        p999Latencies.append(float(row[5]))
                        for x in range(priorities):
                            #print(x, float(row[5+(2*x)]))
                            p95PrioLatencies[x].append(float(row[7+(4*x)])) 
                            p99PrioLatencies[x].append(float(row[8+(4*x)]))   
                            p999PrioLatencies[x].append(float(row[9+(4*x)]))     

    #print(load_list)
    #print(p99_list)
 

figure, ((ax)) = plt.subplots(2, 1)
ax[0].plot(load_list, p99Latencies, '-' , marker='x', label="all")
ax[1].plot(load_list, p999Latencies, '-' , marker='x', label="all")

for x in range(priorities):
    if(x % 5 == 0):
        ax[0].plot(load_list, p99PrioLatencies[x], label=x)
        ax[1].plot(load_list, p999PrioLatencies[x], label=x)

ax[0].legend()
ax[1].legend()

ax[0].set_ylabel('p99 latency (ns)')
ax[1].set_ylabel('p999 latency (ns)')#, fontsize=20)
ax[0].set_xlabel('Load (RPS)') 
ax[1].set_xlabel('Load (RPS)') 
ax[0].set_ylim([0, 1e5])
ax[1].set_ylim([0, 1e5])
figure.set_size_inches(8, 8)
#plt.show()
plt.savefig(input_dir+'/graph/'+path[1]+'_priority_plot.pdf')#,bbox_inches='tight')

