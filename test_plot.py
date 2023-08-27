import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from csv import reader

import pandas as pd
import json
import matplotlib.ticker as mtick
from matplotlib import cm

queues = 64
trafficpatterns = ["PC", "NC", "EXP"]
#mechanisms = ["IDEAL", "SW", "INORDER", "LZCNT", "AVX","BLINDPOLL"]
#mechanisms = ["SW", "INORDER", "LZCNT", "BLINDPOLL", "IDEAL", "IDEALmany", "AVX"]
mechanisms = ["LZCNT","SW3","INORDER3","IDEAL64"]

# Generate data for the surface plot
#x = np.linspace(-5, 5, 100) #priority

for tp in trafficpatterns:
    #figure, ((ax)) = plt.subplots(2, 1)
    print(tp)
    for mechanism in mechanisms:
        print(mechanism)
        if(mechanism == "IDEAL"): result_file = "result/"+mechanism+"/SQ-result.csv"
        else: result_file = "result/"+mechanism+"/"+tp+"-result.csv"

        #color_index = 0
        #for result_file in file_list:

            #print(result_file)
            #load_list = []
        print(result_file)
        load_list = []
        p50  = []
        p95  = []
        p99  = []
        p999 = []
        priority = []
        
        for i in range(queues):
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
                        #if(float(row[0]) >= 675000): continue

                        #if(float(row[8+(4*i)]) <= 100):
                            load_list.append(float(row[1])/1000) # Convert to MRPS
                            #p50.append(float(row[2]))
                            #p95.append(float(row[3]))
                            #p99.append(float(row[4]))
                            #p999.append(float(row[5])) 
                            priority.append(i)

                            p95.append(float(row[7+(4*i)])) 
                            p99.append(float(row[8+(4*i)]))   
                            p999.append(float(row[9+(4*i)]))   

            #print(load_list)
            #print(p99_list)

        #ax[0].plot(load_list, p95, '-' , marker='x', label=mechanism)
        #ax[1].plot(load_list, p99,'-' , marker='x', label=mechanism)

        #ax[0].legend()
        #ax[1].legend()

        #ax[0].set_ylabel('p95 latency (us)')
        #ax[1].set_ylabel('p99 latency (us)')#, fontsize=20)
        #ax[0].set_xlabel('Load (RPS)') 
        #ax[1].set_xlabel('Load (RPS)') 
        #ax[0].set_ylim([0, 1e2])
        #ax[1].set_ylim([0, 1e2])
        #ax[0].set_title(tp)

        #figure.set_size_inches(8, 8)
        #plt.show()
        #plt.savefig("result/"+tp+"_plot_3.pdf")#,bbox_inches='tight')

        #y = np.linspace(-5, 5, 100) #load
        #X, Y = np.meshgrid(x, y) 
        #Z = np.sin(np.sqrt(X**2 + Y**2)) #latency

        # Create a 3D figure and axes
        fig = plt.figure()
        ax = fig.add_subplot(111, projection='3d')

        # Create the surface plot
        #surf = ax.plot_surface(priority, load_list, p95, cmap='viridis')
        surf = ax.plot_trisurf(load_list, priority, p99, cmap=cm.coolwarm, linewidth=0, antialiased=False)


        # Customize the plot
        ax.set_xlabel('Load (kRPS)')
        ax.set_ylabel('Priority')
        ax.set_zlabel('p95 Latency (us)')
        ax.set_title(mechanism+'_'+tp)
        ax.set_zlim(0, 100)

        # Add a colorbar
        fig.colorbar(surf) #ticks=range(0, 100, 10)

        # Display the plot
        #plt.show()

        plt.savefig('3D_plot_'+mechanism+'_'+tp+'_'+'.pdf')#,bbox_inches='tight')
