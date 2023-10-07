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
#parser = argparse.ArgumentParser()

# Adding optional argument
#parser.add_argument("-f", "--filePath", help = "path to pcap file")
#parser.add_argument("-d", "--directory", help = "path to csv file")

# Read arguments from command line
'''
args = parser.parse_args()
input_dir = args.directory
if input_dir[-1] == "/":
    input_dir = input_dir[:-1]

path = input_dir.split("/")
'''

#if not os.path.exists('graphs'):
#    os.makedirs('graphs')

queues = 64
#trafficpatterns = ["FB", "PC", "SQ", "EXP"]
trafficpatterns = ["FB"]#, "PC", "EXP"]


#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]
#mechanisms = ["SW128", "INORDER128", "LZCNT128"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany","IDEAL64Q2buf","IDEAL1Q128buf","LZCNT64Q2buf","SW64Q2buf"]
#mechanisms = ["SW", "LZCNT", "IDEAL", "IDEAL64", "IDEAL64Q2buf","LZCNT64Q2buf","SW64Q2buf"]

#mechanisms = ["IDEAL8Q2B","LZCNT8Q2B", "LZCNT8Q100B", "IDEAL8Q100B","SW8Q100B"]
#mechanisms = ["SW8Q100B", "LZCNT8Q100B", "IDEAL8Q100B"]
#mechanisms = ["SW", "LZCNT", "IDEAL64"]

#mechanisms = ["SW8Q8X2", "LZCNT8Q8X", "SW8Q8X", "SW8Q8F2", "LZCNT8Q8F", "SW8Q8F"]
#mechanisms = ["SW8Q8X2", "LZCNT8Q8X", "SW8Q8F2", "LZCNT8Q8F"]

mechanisms = ["64QP16CQ16T","64QP16CQ16Tlocalprio","64QP16CQ16Tglobalprio","64QP64CQ16TSUPP","64QP64CQ16TRRCAP"]
legend = ["no prioritizing (ideal)", "local prioritizing", "global prioritizing","scale-up", "ceiling64"]

#priorities = 50
# Creates a list containing 50 lists, each of 0 items, all set to 0
#w, h = 0, priorities

#load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]

#p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
#p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

#file_list = [folder+"d-FCFS(r)-result.csv", folder+"d-FCFS(rr)-result.csv", folder+"JSQ-result.csv", folder+"JLQ-result.csv"]

for tp in trafficpatterns:
    figure, ((ax)) = plt.subplots(2, 1)
    print(tp)
    i = 0
    for mechanism in mechanisms:
        print(mechanism)
        if(mechanism == "IDEAL" or mechanism == "IDEAL64" or mechanism == "IDEAL1Q128buf"): result_file = "result/"+mechanism+"/SQ-result.csv"
        else: result_file = "result/"+mechanism+"/"+tp+"-result.csv"

        #color_index = 0
        #for result_file in file_list:

            #print(result_file)
            #load_list = []

        load_list = []
        p50  = []
        p95  = []
        p99  = []
        p999 = []
        with open(result_file, 'r') as read_obj:
            # pass the file object to reader() to get the reader object
            csv_reader = reader(read_obj)
            header = next(csv_reader)
            if header != None:
                for row in csv_reader:
                    #print(row)
                    #if len(row) != 0:
                    #    if row[1] != '':
                        #if float(row[5]) <= 200:
                    load_list.append(float(row[1])) # Convert to MRPS
                    p50.append(float(row[2]))
                    p95.append(float(row[3]))
                    p99.append(float(row[4]))
                    p999.append(float(row[5])) 
            #print(load_list)
            #print(p99_list)

        ax[0].plot(load_list, p95, '-' , marker='', label=legend[i])
        ax[1].plot(load_list, p99,'-' , marker='', label=legend[i])
        i = i + 1

    ax[0].legend()
    ax[1].legend()

    ax[0].set_ylabel('p95 latency (us)')
    ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
    ax[0].set_xlabel('Load (RPS)') 
    ax[1].set_xlabel('Load (RPS)') 
    ax[0].set_ylim([0, 1e3])
    ax[1].set_ylim([0, 1e3])
    ax[0].set_title(tp)
    #ax[0].axhline(10, color='k',linestyle='-.')
    #ax[0].axhline(20, color='k',linestyle='-.')
    #ax[1].axhline(10, color='k',linestyle='-.')
    #ax[1].axhline(20, color='k',linestyle='-.')


    figure.set_size_inches(8, 8)
    #plt.show()
    plt.savefig("result/"+tp+"_plot_64Q16T_95_99.pdf")#,bbox_inches='tight')
