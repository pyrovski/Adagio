#!/usr/bin/python
 
import getopt, sys, string, os

debug=0
verbose=0
lineno=0
long_listing=0
idle_time=0
reducible_time=0

def getLine(f):
    global lineno
    lineno = lineno + 1
    line = f.readline()

    if len(line) == 0:
        return (0, 0, 0, 99, 0)
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
    check=1
    count=1
    idle_time=1
    reducible_time=1
    lst_time=0
    previous_blk_pre=0.0
    previous_blk_post=0.0
    f_time=0.0
    r_time=0.0
    i_time=0.0
    last_blk_pre_time=0.0
    last_blk_post_time=0.0
    expected1=17
    expected2=0
    init_time=0
    lastevent=0 
    
    print f_time, i_time, r_time
    # find first send
    while 1:
    	(action, time)=parseLine(f,node)
#        print action
	
	if action==99:
		#print "end of file"	
		break
        
	if action == '1':
#		print action
		last_send_pre_time=time
#		lastevent=1
		
     	if action == '2':
		#print lastevent, action
		last_send_post_time=time
		reducible_start_time=time
		if lastevent == 2 or lastevent ==3 or lastevent == 4:
			f_time=time-last_blk_post_time
			init_time=0
			print f_time, r_time, i_time
			r_time=0
			i_time=0
			f_time=0
		lastevent=1
	
	if action == '11':
#		print action
		lastevent=2
		last_wait_pre_time=time
		r_time=r_time+time-last_blk_post_time

	if action == '12':
#		print action
		lastevent=2
		last_blk_post_time=time
		i_time=i_time + time-last_wait_pre_time
		if i_time < 0:
			print "1"
			print i_time,time, last_wait_pre_time
			break;


        if action == '3':
#		print action
		r_time=r_time+time-reducible_start_time
		lastevent=3
		last_wait_pre_time=time
	
	if action == '4':
#		print action
		lastevent=3
		reducible_start_time=time
		i_time=i_time+time-last_wait_pre_time
		if i_time <0:
			print "2"
			print i_time,time,last_wait_pre_time
			break;
		last_blk_post_time=time

	if action == '9':	
		lastevent=4
		r_time=r_time+time-last_blk_post_time
		last_blk_pre_time=time
	
	if action == '10':
		print i_time
		lastevent=4
		i_time=i_time+time -last_blk_pre_time
		if i_time <0:
			print "3"
			print i_time,time,last_blk_pre_time
			break;
		last_blk_post_time=time
		
		


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
     

