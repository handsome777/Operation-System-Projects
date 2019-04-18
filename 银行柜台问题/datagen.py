import random
file = "customer_data.dat"

customer_num = 100

data = []
k = 1
for i in range(customer_num):
	s = []
	s.append(random.randint(1,100))
	s.append(random.randint(1,10))
	data.append(s)

data.sort(key = lambda x:x[0])
#print data

with open(file,'w') as f:
	for i in range(customer_num):
		f.write(str(i));
		f.write(" ")
		f.write(str(data[i][0]))
		f.write(" ")
		f.write(str(data[i][1]))
		f.write('\n')
