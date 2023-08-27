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
parser.add_argument("-t", "--trafficpattern", help = "traffic pattern ID: FB(5), PC(6), SQ(7), NC(8), EXP(9)")
parser.add_argument("-p", "--priorities", help = "number of priorities")

# Read arguments from command line
args = parser.parse_args()
tp = int(args.trafficpattern)
priorities = int(args.priorities)

trafficpatterns = ["NULL", "NULL", "NULL", "NULL", "NULL", "FB", "PC", "SQ", "NC", "EXP"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
mechanisms = ["IDEAL", "SW", "SW_again", "INORDER", "INORDER_again","LZCNT","BLINDPOLL"]

#priorities = 50
# Creates a list containing 50 lists, each of 0 items, all set to 0
#w, h = 0, priorities

#load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]

#p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
#p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

#file_list = [folder+"d-FCFS(r)-result.csv", folder+"d-FCFS(rr)-result.csv", folder+"JSQ-result.csv", folder+"JLQ-result.csv"]

#for tp in trafficpatterns:
figure, ((ax)) = plt.subplots(3, 1)
for mechanism in mechanisms:
    if mechanism == "IDEAL": result_file = mechanism+"/1/7/result_all.csv"
    else: result_file = mechanism+"/"+str(priorities)+"/"+str(tp)+"/result_all.csv"
    #color_index = 0
    #for result_file in file_list:

        #print(result_file)
        #load_list = []

    load_list = []
    #p50  = []
    p95  = []
    p99  = []
    p999 = []
    with open(result_file, 'r') as read_obj:
        # pass the file object to reader() to get the reader object
        csv_reader = reader(read_obj)
        header = next(csv_reader)
        if header != None:
            for row in csv_reader:
                print(row)
                #if len(row) != 0:
                #    if row[1] != '':
                    #if float(row[5]) <= 200:
                load_list.append(float(row[1])) # Convert to MRPS
                #p50.append(float(row[2]))
                p95.append(float(row[3])/1000)
                p99.append(float(row[4])/1000)
                p999.append(float(row[5])/1000) 
        #print(load_list)
        #print(p99_list)

    ax[0].plot(load_list, p95, '-' , marker='x', label=mechanism)
    ax[1].plot(load_list, p99,'-' , marker='x', label=mechanism)
    ax[2].plot(load_list, p999,'-' , marker='x', label=mechanism)

ax[0].legend()
ax[1].legend()
ax[2].legend()

ax[0].set_ylabel('p95 latency (us)')
ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
ax[2].set_ylabel('p999 latency (us)')#, fontsize=20)

ax[0].set_xlabel('Load (RPS)') 
ax[1].set_xlabel('Load (RPS)') 
ax[2].set_xlabel('Load (RPS)') 

#ax[0].set_ylim([0, 1e2])
#ax[1].set_ylim([0, 1e2])
#ax[2].set_ylim([0, 1e2])

ax[0].set_title(tp)

figure.set_size_inches(8, 16)
#plt.show()
plt.savefig(trafficpatterns[tp]+"_"+str(priorities)+"_plot.pdf")#,bbox_inches='tight')