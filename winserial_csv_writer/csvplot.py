import numpy as np
import csv
import matplotlib.pyplot as plt

"""
Read input from csv and truncate off garbage. Obtain:
	a
	b
	t
	Fs
"""
#read csv
rd = []
with open('log.csv', newline='') as csvfile:
	spamreader=csv.reader(csvfile,delimiter=',', quoting=csv.QUOTE_NONNUMERIC)
	for row in spamreader:
		rd.append(row)
		


t = np.array(rd[len(rd)-1])
rd = rd[0:(len(rd)-1)][:]

figs,ax = plt.subplots()
for arr in rd:
	arr = np.array(arr)
	ax.plot(t, arr)
	

fig2, ax2 = plt.subplots()
ax2.plot(rd[3],rd[1])

plt.show()