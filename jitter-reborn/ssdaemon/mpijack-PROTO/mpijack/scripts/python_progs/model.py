#!/usr/bin/python
 
import getopt, sys, string, os

debug=0
verbose=0
lineno=0
long_listing=0
transition = int(1000)                  # default 1000 us
idlePwr = 0.05                          # default 5% of full

def getLine(f):
    global lineno
    lineno = lineno + 1
    line = f.readline()

    if len(line) == 0:
        return (-1.0, -1.0, -1.0)
    
    fields = string.split(line)
    if fields[0] == "#":
        return getLine(f)
    if len(fields) != 3:
        print "input error line", lineno
        sys.exit(-1)
    else:
        return (float(fields[0]), float(fields[1]), float(fields[2]))

# power function
def pwr(r, i):
    # pwr is square of ratio r/(r+i)
    # however, we don't scale below 50% of full
    if (i-transition) > r:
        # run at 0.25 for 2*r then idle
        return 0.5 * r + transition + (i - transition  - r)*idlePwr
    else:
        ratio = r/(r+i-transition)
        return ratio*ratio * (r+i-transition) +transition
    
#
# main routine
#
#
def parseFile(f):
    count = int(0)
    power_down = int(0)
    fullTotal = reduceTotal = idleTotal = float(0)
    powerAdvanced = float(0)
    powerSimple = float(0)
    power_down_full = float(0)
    power_down_idle = float(0)
    power_down_reduce = float(0)

    while 1:
        (full, reduce, idle) = getLine(f)
        if full < 0:
            break
        count += 1
        fullTotal += full
        reduceTotal += reduce
        idleTotal += idle

        if idle < transition:
	    powerSimple += reduce + idle
            powerAdvanced += reduce + idle
        else:
            power_down += 1
            power_down_full += full
            power_down_idle += idle
            power_down_reduce += reduce
            powerSimple += reduce + transition + (idle - transition) * idlePwr
            powerAdvanced += pwr(reduce, idle)

    total = fullTotal+reduceTotal+idleTotal
    powerSimple += fullTotal
    powerAdvanced += fullTotal
    
    # print results
    print "Total cycles:", count
    print "Powered down: %d (%2.3g %%)" \
          % (power_down,float(power_down)/count*100)
    print
    print "             Total           Full      Reducible           Idle"
    print "Time: %12g = %12g + %12g + %12g" \
          % (total,  fullTotal, reduceTotal, idleTotal)
    print "Aver: %12g = %12g + %12g + %12g" % \
          (total/count, fullTotal/count, reduceTotal/count, idleTotal/count)
    print "P.D.: %12g = %12g + %12g + %12g" % \
          ((power_down_full+power_down_reduce+power_down_idle)/power_down, \
           power_down_full/power_down, power_down_reduce/power_down, \
           power_down_idle/power_down)
    print
    print "Power Full    : %12g" % total
    print "Power Simple  : %12g (%2.3g %%)" % \
          (powerSimple, powerSimple/total*100)
    print "Power Advanced: %12g (%2.3g %%)" % \
          (powerAdvanced, powerAdvanced/total*100)
    

optstring="hdvo:lt:i:"
longopts=["help", "debug", "verbose", "output=", "node=", "long", "transition"
          "idle"]
 
def usage():
    print sys.argv[0], "-[", optstring, "] files"
    print """
        -v, --verbose:          progress message (multiple times)
        -d, --debug:            debugging output
        -h, --help:             this message
        -o, --output <file>	write output to file
        -t, --transition <ms>	transition time between op pts in mseconds
        -i, --idle <%>		power idle state uses (relative to full) [0:1]
          """
 
def main():
    global debug, verbose, long_listing, transition, idlePwr
    out = sys.stdout
    
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
        elif o in ("-l", "--long"):
            long_listing = 1
        elif o in ("-t", "--transition"):
            transition = int(a) * 2
            if transition < 0:
                print "transition value negative", transition
                sys.exit(-1)
        elif o in ("-i", "--idle"):
            idlePwr = int(a)
            if idlePwr < 0 or idlePwr > 1:
                print "invalid idlePwr value", idlePwr
                sys.exit(-1)
        else:
            print "invalid option", o
            sys.exit(-1)

    if len(args) < 1:
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
 
        if debug>0: print "opening", a

        if verbose:
            # print header
            print "# generated by", sys.argv[0], "at ",
            sys.stdout.flush()
            os.system("date")
            print "#"
            print "# input file:", os.getcwd()+'/'+a
            print "#"
            
        print "Parameters: transition cycle", transition, "(ms)",
        print "idle pwr", idlePwr*100, "%"
        parseFile(f)
 
 
if __name__ == "__main__":
    main()
     

