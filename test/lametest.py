#!/usr/bin/python
from string import *
import os, commands, getopt, sys


def Usage(mesg):
    print mesg
    print "\nUsage: "
    print "\nMode 1. Compare output of 'lame1' and 'lame2':"
    print "./lametest.py  options_file  input.wav lame1 lame2"
    print "\nMode 2. Compare output of lame1 with reference solutions:"
    print "./lametest.py options_file input.wav lame1"
    print "\nMode 3. Generate reference solutions using lame1:"
    print "./lametest.py -m options_file input.wav lame1"
    sys.exit(0)



##################################################################
# compare two files, return # bytes which differ and the
# number of bytes in larger file 
##################################################################
def fdiff(name1,name2):
    cmd = "cmp -l "+name1 + " " + name2 + " | wc --lines"
    out = commands.getoutput(cmd)
    out = split(out,"\n")
    out=out[-1]
    if (-1==find(out,"No")):
        diff = atof(out)
        size1 = os.path.getsize(name1)
        size2 = os.path.getsize(name2)
        diff = diff + abs(size1-size2)
        size = max(size1,size2) 
    else:
        diff = -1
        size = 0
    return (diff,size)

################################################################## 
# compare two files, return # bytes which differ and the
# number of bytes in larger file 
##################################################################
def compare(name1,name2,decode):
    if (decode):
        print "converting mp3 to wav for comparison..."
        commands.getoutput("lame --mp3input --decode " + name1)
        commands.getoutput("lame --mp3input --decode " + name2)
        name1 = name1 + ".wav"
        name2 = name2 + ".wav"

    rcode = 0
    diff,size=fdiff(name1,name2)
    if (diff==0):
        print "output identical:  diff=%i  total=%i" % (diff,size)
        rcode = 1
    elif (diff>0):
	print "output different: diff=%i  total=%i  %2.0f%%" % \
        (diff,size,100*float(diff)/size)
    else:
	print "Error comparing files:"
        print "File 1: " + name1
        print "File 2: " + name2

    return rcode






################################################################## 
# main program
##################################################################
try:
    optlist, args = getopt.getopt(sys.argv[1:], 'wm')
except getopt.error, val:
    Usage('ERROR: ' + val)

decode=0
lame2="none"
for opt in optlist:
    if opt[0] == '-w':
        decode=1
    elif opt[0] == '-m':
        lame2="makeref"
        print "\nGenerating reference output"


if len(args) < 3:
    Usage("Not enough arguments.")
if len(args) > 4:
    Usage("Too many arguments.")

if (lame2=="makeref"):
    if len(args) != 3:
    	Usage("Too many arguments for -r/-m mode.")
else:
    if len(args) ==3:	
        lame2="ref"

if len(args) >=3:
    options_file = args[0]
    input_file = args[1]
    lame1 = args[2]
if len(args) >=4:
    lame2 = args[3]

status,output=commands.getstatusoutput("which "+lame1)
if 0 != status:
    Usage("Executable "+lame1+" does not exist")

if not (lame2=="ref" or lame2=="makeref"):
    status,output=commands.getstatusoutput("which "+lame1)
    if 0 != status:
        Usage("Executable "+lame2+" does not exist")

basename = replace(input_file,".wav","")
basename = basename + "." + options_file


num_ok=0
n=0
foptions=open(options_file)
line = rstrip(foptions.readline())
while line:
    n = n+1
    name1 = basename + "." + str(n) + ".mp3"  
    name2 = basename + "." + str(n) + "ref.mp3"  
    if (lame2=='ref'):
        cmd = lame1 + " " + line + " " + input_file + " " + name1
        print "\nexecutable:      ",lame1
        print   "options:         ",line
        print   "input:           ",input_file 
        print   "reference output:",name2
        commands.getoutput(cmd)
        num_ok = num_ok+compare(name1,name2,decode)
    elif (lame2=='makeref'):
        print "\nexecutable: ",lame1
        print   "options:    ",line
        print   "input:      ",input_file 
        print   "output:     ",name2
        cmd = lame1 + " " + line + " " + input_file + " " + name2
        commands.getoutput(cmd)
    else:
        print "\nexecutable:  ",lame1
        print   "executable2: ",lame2
        print   "options:     ",line
        print   "input:       ",input_file
        cmd1 = lame1 + " " + line + " " + input_file + " " + name1
        cmd2 = lame2 + " " + line + " " + input_file + " " + name2
        commands.getoutput(cmd1)
        commands.getoutput(cmd2)
	num_ok = num_ok + compare(name1,name2,decode)

    line = rstrip(foptions.readline())

foptions.close()
print "\nNumber of tests which passed: ",num_ok
print "Number of tests which failed: ",n-num_ok
