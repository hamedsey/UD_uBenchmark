import numpy as np
import matplotlib.pyplot as plt
x = [1,2,4,8,16,32,64,128,256]
y1 = [695430,677900,655597,646746,612274,607867,600517,599539,575068]

y2 = [76,79,82,84,86,88,88,92,97]

fig, ax1 = plt.subplots()

ax2 = ax1.twinx()
ax1.plot(x, y1, 'g-')
ax2.plot(x, y2, 'b-')

ax1.set_xlabel('Number of Queues')
ax1.set_ylabel('Throughput (RPS)', color='g')
ax2.set_ylabel('Clk Cycles to Find Work', color='b')
fig.set_size_inches(3, 3)
plt.savefig('plot2yaxes.pdf',bbox_inches='tight')
#plt.show()
