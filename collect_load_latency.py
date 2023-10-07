import numpy as np
from statistics import mean
import argparse
import csv
import os, glob
import sys


# Initialize parser
parser = argparse.ArgumentParser()

# Adding optional argument
parser.add_argument("-i", "--index", help = "index corresponding to notification mechanism (e.g. IDEAL(0),SW(1),INORDER(2),LZCNT(3),AVX(4),BLINDPOLL(5))")
parser.add_argument("-t", "--trafficPattern", help = "index corresponding to traffic pattern (e.g. FB(5),PC(6),SQ(7),NC(8),EXP(9))")
parser.add_argument("-q", "--queues", help = "number of queues")


# Read arguments from command line
args = parser.parse_args()
index = int(args.index)
id = int(args.trafficPattern)
#queues = int(args.queues)

queuesList = [48] #[8,16,32,64,128,256]

#mechanisms = ["IDEAL128", "SW128", "INORDER128", "LZCNT128", "AVX", "BLINDPOLL","IDEALmany","IDEAL64Q2buf","IDEAL1Q128buf","LZCNT64Q2buf","SW64Q2buf","IDEAL8Q2B","LZCNT8Q2B", "LZCNT8Q100B", "IDEAL8Q100B","SW8Q100B"]
#mechanisms = ["LZCNT8Q8X","LZCNT8Q8F","SW8Q8X","SW8Q8X2","SW8Q8F2","SW8Q8F","JLQ16SW64","16Q64P","16Q64P30","dFCFS", "dFCFS++", "SUPP","SUPP-Qsweep","64QP16CQ16T","64QP16CQ16Tlocalprio","64QP16CQ16Tglobalprio","64QP64CQ16TSUPP","64QP64CQ16TRRCAP", "64SW"]
mechanisms = ["48QP12CQ12Tlocalprio","48QP48CQ12TlocalRRprio","48QP48CQ12TSUPP","48QP12CQ12Tglobalprio","48QP1CQ12Tglobalprio"]
#last_index = 18
#20M, 20M, 1M, 500k, 500k

#Create results and graphs folders
if not os.path.exists('result'):
    os.makedirs('result')
	#for mechanism in mechanisms:
    os.makedirs('result/'+mechanisms[index])
          
if not os.path.exists('result/'+mechanisms[index]):
    os.makedirs('result/'+mechanisms[index])

clk_period = 4.6

#trafficPatternID = [6,7,8,9]
trafficpatterns = ["NULL", "NULL", "NULL", "NULL", "NULL", "FB", "PC", "SQ", "NC", "EXP"]
#generatedLoad = ["50000","100000","200000","300000","400000","425000", "450000", "475000", "500000", "525000", "550000", "575000", "600000", "625000", "650000", "675000", "700000", "725000", "750000", "800000"]
#generatedLoad = ["1000000","2000000","4000000","8000000","12000000","16000000", "18000000", "20000000", "22000000", "24000000", "26000000", "28000000"]
#generatedLoad =  ["1000000","2000000","3000000","4000000","6000000","8000000","10000000","12000000","14000000","16000000","18000000","20000000","22000000","24000000","26000000","28000000"]

#generatedLoad = ["1000000","2000000","3000000","4000000","6000000","8000000","9000000","10000000","11000000","12000000","13000000","14000000","15000000","16000000","17000000","18000000","19000000","20000000"]
generatedLoad = ["50000", "100000", "150000", "200000", "250000", "300000", "350000" ,"400000" ,"450000" ,"500000"]
#generatedLoad = ["100000", "200000", "300000", "400000", "500000", "600000", "700000", "800000", "900000", "1000000"]


#for load in 50000 100000 200000 300000 400000 425000 450000 475000 500000 525000 550000 575000 600000 625000 650000 675000 700000 725000 750000 775000 800000;do

#generatedLoad = ["500000","525000","550000","575000","600000","625000","650000", "675000", "700000", "725000", "750000", "800000"]


mechanism = mechanisms[index]
#for mechanism in mechanisms: 
#for id in trafficPatternID:
print("now doing "+trafficpatterns[id])
#load_list = []
p50_list  = []
p95_list  = []
p99_list  = []
p999_list = []


for queues in queuesList:
    print(queues)
    #with open('result/'+mechanism+'/'+trafficpatterns[id]+'-'+mechanism+'-'+str(queues)+'-result.csv', 'w') as f:
    with open('result/'+mechanism+'/'+trafficpatterns[id]+'-result.csv', 'w') as f:

        # create the csv writer
        writer = csv.writer(f)

        # write a row to the csv file
        writer.writerow(["client load","measured load","median","95%", "99%", "99.9%", "lost too much"])
        for clientLoad in generatedLoad:
        #trafficpatterns[id]
            results = []
            results.append(clientLoad)

            file_list = glob.glob(mechanism+"/"+str(queues)+"/"+str(id)+"/"+str(clientLoad)+"/all_*.result");
            count = 0
            allData = []
            data = []

            for filename in file_list:
                print(filename)
                if count == 0:
                    name, ext = filename.split(".")
                    priority, measuredLoad = name.split("_")
                    results.append(int(measuredLoad))
                    #load_list.append(int(measuredLoad))
                    count = count + 1

                #print(filename)
                data = np.loadtxt(filename)
                allData = np.append(allData, data)

            #print(len(allData))
            results.append((clk_period/1000)*np.percentile(allData, 50))
            results.append((clk_period/1000)*np.percentile(allData, 95))
            results.append((clk_period/1000)*np.percentile(allData, 99))
            results.append((clk_period/1000)*np.percentile(allData, 99.9))

            if(id != 7):
                #now parsing other priorities
                for x in range(queues):
                    filePath = mechanism+"/"+str(queues)+"/"+str(id)+"/"+str(clientLoad)+"/"+str(x)+"_*.result"
                    file_list = glob.glob(filePath)
                    if not os.path.exists(file_list[0]): continue

                    allData = []
                    data = []

                    for filename in file_list:
                        print(filename)

                        #print(filename)
                        data = np.loadtxt(filename)
                        allData = np.append(allData, data)

                    if data.size == 0 or data.size < 4: 
                        print("data empty")
                        results.append(0)
                        results.append(0)
                        results.append(0)
                        results.append(0)
                    else:
                        results.append((clk_period/1000)*np.percentile(allData, 50))
                        results.append((clk_period/1000)*np.percentile(allData, 95))
                        results.append((clk_period/1000)*np.percentile(allData, 99))
                        results.append((clk_period/1000)*np.percentile(allData, 99.9))

            writer.writerow(results)

'''
# Initialize parser
parser = argparse.ArgumentParser()

# Adding optional argument
parser.add_argument("-d", "--directory", help = "input and output directory")

# Read arguments from command line
args = parser.parse_args()
input_dir = args.directory
if input_dir[-1] == "/":
    input_dir = input_dir[:-1]

folder_path_list = glob.glob(input_dir+"*")
if len(folder_path_list) == 0:
    sys.exit('Input directory incorrect')

for folder_path in folder_path_list:
    print(folder_path)
    file_list = glob.glob(folder_path+"/*.result") 
    file_list = sorted(file_list,key = os.path.getmtime)
    results = [[]]
    load_list = []
    p50_list  = []
    p95_list  = []
    p99_list  = []
    p999_list = []
    for filename in file_list:
        #print(filename)
        data = np.loadtxt(filename)
        data_mean = mean(data)
        maximum = max(data)
        p50 = np.percentile(data, 50)
        p95 = np.percentile(data, 95)
        p99 = np.percentile(data, 99)
        p999 = np.percentile(data, 99.9)
        config = filename.split('/')[1].split('.')[0]
        configs = config.split('_')
        result = [configs[0], configs[1], configs[2], p50, p95, p99, p999]
        results.append(result)
        load_list.append(int(configs[2])/1000000)
        p50_list.append(p50)
        p95_list.append(p95)
        p99_list.append(p99)
        p999_list.append(p999)
    with open('result/'+folder_path+'-result.csv', 'w') as f:
        # create the csv writer
        writer = csv.writer(f)

        # write a row to the csv file
        writer.writerow(["client load", "measured load","median","95%", "99%", "99.9%"])
        writer.writerows(results)
'''