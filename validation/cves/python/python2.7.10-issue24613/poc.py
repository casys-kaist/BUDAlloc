import array
import sys
import random

testNumber = 0

def dump(value):
	global testNumber
	i = 0
	for x in value:
		y = ord(x)
		if (y != 0x41): 
			end = ''.join(value[i:]).index('A' * 0x10)
			sys.stdout.write("%08x a[%08x]: " % (testNumber, i))
			for z in value[i:i+end]: sys.stdout.write(hex(ord(z))[2:])
			sys.stdout.write('\r\n')
			break			
		i += 1

def copyArray():
	global testNumber
	while True:
		a=array.array("c",'A'*random.randint(0x0, 0x10000))
		a.fromstring(a)
		dump(a)
		testNumber += 1
	
print "Starting..."	
copyArray()
