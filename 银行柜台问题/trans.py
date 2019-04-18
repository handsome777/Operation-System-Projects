filename = "result.txt"
filename1 = "trans.txt"

g = open(filename1,'w');

with open(filename,'r') as f:
	for line in f:
		line.strip('\n')
		g.write(line)
		g.write("\\\\")
		g.write('\n')
