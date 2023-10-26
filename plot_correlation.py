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
import scipy.stats as stats

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



priorities = 48

trafficpatterns = ["FB", "PC", "EXP"]#, "PCR"] #["PC", "EXP", "NC"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]
#mechanisms = ["LZCNT"] 
#mechanisms = ["LZCNT","SW3","INORDER3"]
#mechanisms = ["SW3", "INORDER3", "LZCNT", "IDEAL64"]
#mechanisms = ["JLQ16SW64","16Q64P","16Q64P30"]
#mechanisms = ["dFCFS++"] #["SUPP"]
#mechanisms = ["64QP16CQ16T","64QP16CQ16Tlocalprio","64QP16CQ16Tglobalprio","64QP64CQ16TSUPP","64QP64CQ16TRRCAP"]
#legend = ["no prioritizing (ideal)", "local prioritizing", "global prioritizing","scale-up","ceiling64"]


#global prioritizing
#priomethod = "global"
#mechanisms = ["48QP1CQ12Tglobalprio","48QP12CQ12Tglobalprio","48QP48CQ12TSUPP"]
#legend = ["single-queue", "12 queues", "48 queues"]

#local prioritizing
#priomethod = "local"
#mechanisms = ["48QP12CQ12Tlocalprio","48QP48CQ12TlocalRRprio"]
#legend = ["12 queues", "48 queues"]

#group prioritizing
#priomethod = "group"
#mechanisms = ["48QP12CQ12Tgroupsof4prio","48QP6CQ12Tgroupsof4prio","48QP3CQ12Tgroupsof4prio","48QP6CQ12Tgroupsof2prio"]
#legend = ["12 queues - 3 groups", "6 queues - 3 groups", "3 queues - 3 groups", "6 queues - 6 groups"]

#--------------------------
###################################
#fixed sevice time (has all four traffic patterns)

#global prioritizing
#priomethod = "global"
#mechanisms = ["1Q12Tglobalprio","48Q12TSUPP"]
#legend = ["1 queue", "48 queues"]

#local prioritizing
#priomethod = "local"
#mechanisms = ["12Q12Tlocalprio","48Q12Tlocalprio"]
#legend = ["12 queues", "48 queues"]

#group prioritizing
#priomethod = "group"
#mechanisms = ["6Q12Tgroupprio","3Q12Tgroupprio"]
#legend = ["6 queues", "3 queues"]

###################################
#exponential service time (only has FB, EXP, and PC)

#global prioritizing
priomethod = "global"
mechanisms = ["1Q12TglobalprioEXP","48Q12TglobalprioSUPPEXP"]#,"ceilingEXP"]
legend = ["1 queue", "48 queues"]#, "ceiling"]

#local prioritizing
#priomethod = "local"
#mechanisms = ["12Q12TlocalprioEXP","48Q12TlocalprioEXP"]#, "ceilingEXP"]
#legend = ["12 queues", "48 queues"]#, "ceiling"]

#group prioritizing
#priomethod = "group"
#mechanisms = ["6Q12TgroupprioEXP","3Q12TgroupprioEXP"]#, "ceilingEXP"]
#legend = ["6 queues", "3 queues"]#, "ceiling"]
###################################



#colors = ['#C5C9C7', '#C79FEF','#7BC8F6', '#030764']

for tp in trafficpatterns:
    figure, ((ax)) = plt.subplots(2, 1)
    print(tp)
    i = 0
    for mechanism in mechanisms:
        print(mechanism)
        if(mechanism == "IDEAL"): result_file = "result/"+mechanism+"/SQ-result.csv"
        else: result_file = "result/"+mechanism+"/"+tp+"-result.csv"

        #print(result_file)
        load_list = []

        # Creates a list containing 50 lists, each of 0 items, all set to 0
        w, h = 0, priorities

        #load_list = [98078, 199196, 294768, 387378, 489066, 587130, 661236, 663783, 664703, 665868]
        p95PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        p99PrioLatencies  = [[0 for x in range(w)] for y in range(h)]
        #p999PrioLatencies = [[0 for x in range(w)] for y in range(h)]

        p95Latencies = []
        p99Latencies = []
        #p999Latencies = []
        rows = 0
        rowb4SLO = 0
        rowafterSLO = 0
        foundSLO = 0
        slo = 50

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
                            #p999Latencies.append(float(row[5]))
                            for x in range(priorities):
                                #print(8+(4*x))
                                p95PrioLatencies[x].append(float(row[7+(4*x)])) 
                                p99PrioLatencies[x].append(float(row[8+(4*x)]))   
                                #p999PrioLatencies[x].append(float(row[9+(4*x)]))     

                            rows = rows + 1


        #print(load_list)
        #print(p99_list)

        for index in range(rows):
            if foundSLO == 0 and float(p95PrioLatencies[priorities-1][index]) >= slo:
                if index < rows - 1:
                    if p95PrioLatencies[priorities-1][index] > p95PrioLatencies[priorities-1][index+1]: continue #avoids latency jitter at low loads
                print(p95PrioLatencies[priorities-1][index])
                foundSLO = 1
                rowafterSLO = index
                if index == 0: rowb4SLO = index
                else: rowb4SLO = index - 1


        #ax[0].plot(load_list, p99Latencies, '-' , marker='x', label="all")
        #ax[1].plot(load_list, p999Latencies, '-' , marker='x', label="all")
        print("rows = "+str(rows))
        max_coef = 0 
        for index in range(rows):
            #print(index)
            satLatP99 = []
            satLatP95 = []
            priorityList = []
            for x in range(priorities):
                priorityList.append(x)
                satLatP95.append(p95PrioLatencies[x][index])
                satLatP99.append(p99PrioLatencies[x][index])    

            my_rho = np.corrcoef(priorityList, satLatP95)
            tau, p_value = stats.kendalltau(priorityList, satLatP95)
            coef, p = stats.spearmanr(priorityList, satLatP95)

            print(str(load_list[index]) + " , " + str(p95Latencies[index]) + " , " + str(my_rho[0][1]) + " ... kendall: " + str(tau) + " ... spearman's: " + str(coef))
            if float(my_rho[0][1]) > max_coef: 
                max_coef = float(my_rho[0][1])
                max_coef_index = index

        print("row b4 SLO = " + str(rowb4SLO) + "   row after SLO = " + str(rowafterSLO))
        #print("max coef = " + str(max_coef) + "i = "+ str(max_coef_index))
        #satLatP999 = []
        satLatP99 = []
        satLatP95 = []
        priorityList = []
        index = rowb4SLO #rows-3 #15#7 #10 #16 #for 700k load
        for x in range(priorities):
            priorityList.append(x)
            satLatP95.append(p95PrioLatencies[x][index])
            satLatP99.append(p99PrioLatencies[x][index])
            #satLatP999.append(p999PrioLatencies[x][index])

        #ax[0].axhline(y=p99Latencies[index], color='r', linestyle='-')

            #if(x % 5 == 0): 
        #if mechanism == "SW3" or mechanism == "LZCNT": print(satLatP99)
        #ax[0].legend()
        #ax[1].legend()

        #print(priorityList)
        #print(satLatP95)
        #print(satLatP99)

        #Calculating Pearson's Correlation Coefficient
        my_rho = np.corrcoef(priorityList, satLatP95)
        tau, p_value = stats.kendalltau(priorityList, satLatP95)
        coef, p = stats.spearmanr(priorityList, satLatP95)

        #print(my_rho[0][1])
        ax[0].set_title(tp)#+"..."+str(my_rho[0][1]))#, fontsize=20)
        ax[0].plot(priorityList, satLatP95, label=legend[i]+' '+str(my_rho[0][1])+'... k= '+str(tau)+'... s = '+str(coef))


        my_rho = np.corrcoef(priorityList, satLatP99)
        tau, p_value = stats.kendalltau(priorityList, satLatP99)
        coef, p = stats.spearmanr(priorityList, satLatP99)

        #print(my_rho[0][1])
        #ax[1].set_title(my_rho)#+"..."+str(my_rho[0][1]))#, fontsize=20)
        ax[1].plot(priorityList, satLatP99, label=legend[i]+' '+str(my_rho[0][1])+'... k= '+str(tau)+'... s = '+str(coef))
        i = i + 1


    ax[0].legend()
    ax[1].legend()


    ax[0].set_ylabel('p95 latency (us)')
    ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
    #ax[0].set_xlabel('Priorities') 
    ax[1].set_xlabel('Priorities') 
    #ax[0].set_ylim([0, 1.01*max(satLatP95)])
    #ax[1].set_ylim([0, 1.01*max(satLatP99)])

    ax[0].set_ylim([0, 50])
    ax[1].set_ylim([0, 50])

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
    plt.savefig("result/"+tp+"_"+str(priorities)+"P_"+priomethod+"_prio_lat_EXP.pdf")#,bbox_inches='tight')
