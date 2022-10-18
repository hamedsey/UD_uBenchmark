import numpy as np
from statistics import mean
import argparse
import csv
import os, glob
import sys

#Create results and graphs folders
if not  os.path.exists('result'):
	os.makedirs('result')

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
        writer.writerow(["window_size","num_threads","load","median","95%", "99%", "99.9%"])
        writer.writerows(results)
