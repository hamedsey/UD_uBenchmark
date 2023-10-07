import numpy as np
import matplotlib.pyplot as plt
import os, glob
import sys

#data64 = np.loadtxt('JLQ16SW64/64/9/1000000/all_942279.result')
#data64_orig = np.loadtxt('64QP64CQ16TRRCAP/64/5/1000000/all_729257.result')
#data64_orig = np.loadtxt('test/all_44953.result')
#data64_orig = np.loadtxt('test/all_44798.result')

data64_orig = np.loadtxt('test/all_48902.result')

#file_list = glob.glob("test/all_*.result")


clk_period = 4.6
data64 = data64_orig*clk_period/1000
print(np.percentile(data64, 0))
print(np.percentile(data64, 10))
print(np.percentile(data64, 25))
print(np.percentile(data64, 50))
print(np.percentile(data64, 95))
print(np.percentile(data64, 99))
#print(max(data64))
data_size64=len(data64)

# Set bins edges
data_set64=sorted(set(data64))
bins64=np.append(data_set64, data_set64[-1]+1)

# Use the histogram function to bin the data
counts64, bin_edges64 = np.histogram(data64, bins=bins64, density=False)

counts64=counts64.astype(float)/data_size64

# Find the cdf
cdf64 = np.cumsum(counts64)

plt.plot(bin_edges64[0:-1], cdf64,linestyle='--', marker="o", color='g')


plt.ylim((0.98,1))
plt.xlim((0,200)) #use 20 for gets
plt.ylabel("CDF")
plt.xlabel("Latency")
#plt.legend(["Query Length (64)", "Query Length (32)","Query Length (16)","Query Length (8)","Query Length (4)","Query Length (2)","Query Length (1)"], loc ="lower right")
plt.title("all CDF")
plt.grid(True)


plt.show()


# Plot the cdf
plt.savefig('CDF_plot.pdf')#,bbox_inches='tight')

