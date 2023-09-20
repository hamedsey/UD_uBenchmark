'''
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
#parser.add_argument("-d", "--directory", help = "path to csv file")

# Read arguments from command line
args = parser.parse_args()
#input_dir = args.directory
#if input_dir[-1] == "/":
#    input_dir = input_dir[:-1]

#path = input_dir.split("/")



priorities = 64
trafficpatterns = ["PC", "EXP", "NC"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]
mechanisms = ["LZCNT"]


#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

for tp in trafficpatterns:
    figure, ((ax)) = plt.subplots(2, 1)
    print(tp)
    for mechanism in mechanisms:
        print(mechanism)
        if(mechanism == "IDEAL"): result_file = "result/"+mechanism+"/SQ-result.csv"
        else: result_file = "result/"+mechanism+"/"+tp+"-result.csv"

        print(result_file)
        load_list = []

        # Creates a list containing 50 lists, each of 0 items, all set to 0
        w, h = 0, priorities

        #load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]
        p95PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

        p95Latencies = []
        p99Latencies = []
        p999Latencies = []

        with open(result_file, 'r') as read_obj:
            # pass the file object to reader() to get the reader object
            csv_reader = reader(read_obj)
            header = next(csv_reader)
            if header != None:
                for row in csv_reader:
                    if len(row) != 0:
                        if row[1] != '':
                        #if float(row[5]) <= 200:
                            #print(row[1])

                            load_list.append(float(row[1])) # Convert to MRPS
                            p95Latencies.append(float(row[3]))
                            p99Latencies.append(float(row[4]))
                            p999Latencies.append(float(row[5]))
                            for x in range(priorities):
                                #print(8+(4*x))
                                p95PrioLatencies[x].append(float(row[7+(4*x)])) 
                                p99PrioLatencies[x].append(float(row[8+(4*x)]))   
                                p999PrioLatencies[x].append(float(row[9+(4*x)]))     

        #print(load_list)
        #print(p99_list)


        figure, ((ax)) = plt.subplots(2, 1)
        #ax[0].plot(load_list, p99Latencies, '-' , marker='x', label="all")
        #ax[1].plot(load_list, p999Latencies, '-' , marker='x', label="all")

        satLatP999 = []
        satLatP99 = []
        satLatP95 = []
        priorityList = []
        index = 6 #16 #for 700k load
        for x in range(priorities):
            priorityList.append(x)
            satLatP95.append(p95PrioLatencies[x][index])
            satLatP99.append(p99PrioLatencies[x][index])
            satLatP999.append(p999PrioLatencies[x][index])

        ax[0].axhline(y=p99Latencies[index], color='r', linestyle='-')

            #if(x % 5 == 0): 
        ax[0].plot(priorityList, satLatP95, label=x)
        ax[1].plot(priorityList, satLatP99, label=x)

        #ax[0].legend()
        #ax[1].legend()

        #Calculating Pearson's Correlation Coefficient
        my_rho = np.corrcoef(priorityList, satLatP99)
        print(my_rho)
        ax[0].set_title(tp+"..."+str(my_rho[0][1]))#, fontsize=20)

        ax[0].set_ylabel('p95 latency (us)')
        ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
        ax[0].set_xlabel('Priorities') 
        ax[1].set_xlabel('Priorities') 
        ax[0].set_ylim([0, 1.01*max(satLatP95)])
        ax[1].set_ylim([0, 1.01*max(satLatP99)])

        my_rho = np.corrcoef(priorityList, satLatP999)
        print(my_rho)
        ax[1].set_title(tp+"..."+str(my_rho[0][1]))#, fontsize=20)

        #for 50
        #strict
        #ax[0].set_ylim([0, 0.3e7])

        #for 100
        #strict
        #ax[0].set_ylim([0, 3e6])
        #wrr
        #ax[0].set_ylim([0, 2e6])

        #ax[1].set_ylim([0, 1e5])

        figure.set_size_inches(8, 8)
        #plt.show()
        #plt.savefig(input_dir+'/graph/'+path[1]+'_priority_plot.pdf')#,bbox_inches='tight')
        plt.savefig("good_trial/result/"+tp+"_"+mechanism+"_correlation_plot.pdf")#,bbox_inches='tight')
'''

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
#parser.add_argument("-d", "--directory", help = "path to csv file")

# Read arguments from command line
args = parser.parse_args()
#input_dir = args.directory
#if input_dir[-1] == "/":
#    input_dir = input_dir[:-1]

#path = input_dir.split("/")



priorities = 64

trafficpatterns = ["PC", "EXP"] #["PC", "EXP", "NC"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]
#mechanisms = ["LZCNT"] 
#mechanisms = ["LZCNT","SW3","INORDER3"]
#mechanisms = ["SW3", "INORDER3", "LZCNT", "IDEAL64"]
#mechanisms = ["JLQ16SW64","16Q64P","16Q64P30"]
mechanisms = ["dFCFS++"] #["SUPP"]

#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

for tp in trafficpatterns:
    figure, ((ax)) = plt.subplots(2, 1)
    print(tp)

    for mechanism in mechanisms:
        print(mechanism)
        if(mechanism == "IDEAL"): result_file = "result/"+mechanism+"/SQ-result.csv"
        else: result_file = "result/"+mechanism+"/"+tp+"-result.csv"

        print(result_file)
        load_list = []

        # Creates a list containing 50 lists, each of 0 items, all set to 0
        w, h = 0, priorities

        #load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]
        p95PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

        p95Latencies = []
        p99Latencies = []
        p999Latencies = []

        with open(result_file, 'r') as read_obj:
            # pass the file object to reader() to get the reader object
            csv_reader = reader(read_obj)
            header = next(csv_reader)
            if header != None:
                for row in csv_reader:
                    if len(row) != 0:
                        if row[1] != '':
                        #if float(row[5]) <= 200:
                            #print(row[1])

                            load_list.append(float(row[1])) # Convert to MRPS
                            p95Latencies.append(float(row[3]))
                            p99Latencies.append(float(row[4]))
                            p999Latencies.append(float(row[5]))
                            for x in range(priorities):
                                #print(8+(4*x))
                                p95PrioLatencies[x].append(float(row[7+(4*x)])) 
                                p99PrioLatencies[x].append(float(row[8+(4*x)]))   
                                p999PrioLatencies[x].append(float(row[9+(4*x)]))     

        #print(load_list)
        #print(p99_list)


        #ax[0].plot(load_list, p99Latencies, '-' , marker='x', label="all")
        #ax[1].plot(load_list, p999Latencies, '-' , marker='x', label="all")

        satLatP999 = []
        satLatP99 = []
        satLatP95 = []
        priorityList = []
        index = 10 #10 #16 #for 700k load
        for x in range(priorities):
            priorityList.append(x)
            satLatP95.append(p95PrioLatencies[x][index])
            satLatP99.append(p99PrioLatencies[x][index])
            satLatP999.append(p999PrioLatencies[x][index])

        #ax[0].axhline(y=p99Latencies[index], color='r', linestyle='-')

            #if(x % 5 == 0): 
        if mechanism == "SW3" or mechanism == "LZCNT": print(satLatP99)
        #ax[0].legend()
        #ax[1].legend()

        print(priorityList)
        print(satLatP95)
        print(satLatP99)

        #Calculating Pearson's Correlation Coefficient
        my_rho = np.corrcoef(priorityList, satLatP99)
        print(my_rho)
        ax[0].set_title(tp)#+"..."+str(my_rho[0][1]))#, fontsize=20)
        ax[0].plot(priorityList, satLatP95, label=str(my_rho[0][1]))


        my_rho = np.corrcoef(priorityList, satLatP999)
        print(my_rho)
        #ax[1].set_title(my_rho)#+"..."+str(my_rho[0][1]))#, fontsize=20)
        ax[1].plot(priorityList, satLatP99, label=str(my_rho[0][1]))


    ax[0].legend()
    ax[1].legend()


    ax[0].set_ylabel('p95 latency (us)')
    ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
    #ax[0].set_xlabel('Priorities') 
    ax[1].set_xlabel('Priorities') 
    #ax[0].set_ylim([0, 1.01*max(satLatP95)])
    #ax[1].set_ylim([0, 1.01*max(satLatP99)])

    #ax[0].set_ylim([0, 200])
    #ax[1].set_ylim([0, 200])

    #for 50
    #strict
    #ax[0].set_ylim([0, 0.3e7])

    #for 100
    #strict
    #ax[0].set_ylim([0, 3e6])
    #wrr
    #ax[0].set_ylim([0, 2e6])

    #ax[0].set_ylim([0, 120])
    #ax[1].set_ylim([0, 120])

    figure.set_size_inches(8, 8)
    #plt.show()
    #plt.savefig(input_dir+'/graph/'+path[1]+'_priority_plot.pdf')#,bbox_inches='tight')
    plt.savefig("result/"+tp+"_"+mechanism+"_prio_lat.pdf")#,bbox_inches='tight')
