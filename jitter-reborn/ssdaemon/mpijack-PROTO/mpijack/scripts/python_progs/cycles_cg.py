#!/usr/bin/python
 
import getopt, sys, string, os

debug=0
verbose=0
lineno=0
long_listing=0

def getLine(f):
    global lineno
    lineno = lineno + 1
    line = f.readline()

    if len(line) == 0:
        return (0, 0, 0, 9, 0)
    elif line[0][0] == '#':
        return getLine(f)
    
    fields = string.split(line, '|')
    if len(fields) < 5:
        print "input error line", lineno
        sys.exit(-1)
    else:
        return fields[0:5]

def parseLine(f, node):
    (a, b, c, d, time) = getLine(f)

    if debug > 1:
        print a, b, time
    if a == -1:
  	d=-1
	
  #  if b == "D":
  #      event = "D"
  #  elif int(a) == node:
  #      if b == "B":
  #          event = "B"			# set event to blocked
  #     elif b in ("M", "U", "R", "S", "E"):
  #          event = "R"			# set event to running
  #    else:
  #          event = 's'			# set event to send
  #  else:
  #      if int(b) == node:
  #          event = 'r'			# set event to recv
  #      else:
  #          print "unrecognized line", lineno
  #          sys.exit(-1)
  # 
    if debug > 2:
        print d, time*1000000
    return d, float(time)*1000000

#
# main routine
#
# in loop calls parseLine, which reads each line and return event and time
# events are
#  B - blocked
#  R - running
#  s - send
#  r - recv
#  D - done (end of file
#
# state is running (R) or blocked (B)
#
# want to count three times
#   reducible - which is between last send and begin of block
#   idle - the time blocked (following a send)
#   full - time between end of block and last send
#
def parseFile(f, node):
    first_time=1
    full_time=0.0
    reducible_time=0
    idle_time=0
    
    # find first send
    while 1:
        if(first_time==1):
		(pre_send1, pre_send_time1) = parseLine(f, node)
      		(post_send1, post_send_time1) = parseLine(f, node)
		if pre_send1 == 9 or post_send1 == 9:
            		#print "error before first send"
            		sys.exit(-1)
    
    
	(pre_wait, pre_wait_time) = parseLine(f, node)
	   # if pre_wait!=2
	   #	print "error"
           #		sys.exit(-1);
	
    	(post_wait, post_wait_time) = parseLine(f, node)
  	  #if post_wait!=3
    	  # 	print "error"
	  #	sys.exit(-1)
    	
    	(pre_send2,pre_send_time2)=parseLine(f,node)
   	 #if pre_send2 != 0:
     	 #	print "error"
	 #	sys.exit(-1)
	
	(post_send2,post_send_time2)=parseLine(f, node)
  	 # if post_send2 != 1:
     	 #	print "error"
	 #	sys.exit(-1)
                    
	if full_time <0 or reducible_time<0 or idle_time <0:
		print "line_no", lineno
		sys.exit(-1)
  	full_time= post_send_time2- post_wait_time
	reducible_time = pre_wait_time - post_send_time1
	idle_time= post_wait_time - pre_wait_time

	print full_time,reducible_time, idle_time
	pre_send_time1=pre_send_time2
	post_send_time1=post_send_time2
	first_time=0

		    
    
optstring="hdvo:n:l"
longopts=["help", "debug", "verbose", "output=", "node=", "long"]
 
def usage():
    print sys.argv[0], "-[", optstring, "] files"
    print """
        -v, --verbose:          progress message (multiple times)
        -d, --debug:            debugging output
        -h, --help:             this message
        -n, --node <node>	which node number
        -o, --output <file>	write output to file
          """
 
def main():
    global debug, verbose, long_listing
    out = sys.stdout
    node = int(-1)
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], optstring, longopts)
    except getopt.GetoptError:
        # print help information and exit:
        usage()
        sys.exit(2)
 
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-d", "--debug"):
            debug = debug+1
        elif o in ("-v", "--verbose"):
            verbose = verbose+1
        elif o in ("-o", "--output"):
            try:
                out = open(a, 'w')
            except IOError, (error, strerror):
                print strerror+':',args[0]
                sys.exit(-1)
        elif o in ("-n", "--node"):
            node = int(a)
        elif o in ("-l", "--long"):
            long_listing = 1
        else:
            print "invalid option", o
            sys.exit(-1)

    if node < 0:
        print "must set node"
        sys.exit(-2)
        
    if len(args) != 1:
        usage()
        sys.exit(2)
 
    if debug:
        pass

    for a in args:
        try:
            f = open(a, "r");
        except IOError, (error, strerror):
            print strerror+':',args[0]
            sys.exit(-1)
 
        if debug or verbose>0: print "opening", a
 
        # print header
        print "# generated by", sys.argv[0], "at ",
        sys.stdout.flush()
        os.system("date")
        print "#"
        print "# input file:", os.getcwd()+'/'+a
        print "#"
        print "# each line holds three times in microseconds (FULL, REDUCIBLE, IDLE)"
        parseFile(f, node)
 
 
if __name__ == "__main__":
    main()
     

