/**
 * MPI NBody solver with two forms of problem decomposition
 * 
 * @author Tyler Bletsch (tkbletsc@unity.ncsu.edu)
 * For Dr. Vincent Freeh, NCSU CSC Dept. (vin@cs.ncsu.edu)
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "SimpleGraphics.h"
#include <mpi.h>


typedef struct {
    float x;
    float y;
    float z;
} Vector3D;


typedef struct {
    float mass; // kg
    Vector3D p; // position (meters)
    Vector3D v; // velocity (meters)
} Particle;

typedef struct {
	int i; // Index in particle[]
	Particle pt; // Particle itself
} ParticleEntry;

Particle* particle; // Main array of particles
int numParticles; // Size of particle[]
float t; // Current simulation time
int timeSteps; // Number of timesteps to simulate

ParticleEntry* partRecv=NULL; // Array of particles entries received from exchange
ParticleEntry* partSend=NULL; // Array of particles entries to be sent during exchange

int numProcs; // Number of processors involved
int myRank;

static FILE* fpStats; // File stream of the comma-separated-values statistics log

// Frequency of load balance:
//  -1  Use modulus load balancing (free and perfect)
//   0  Use spatial decomp with a geometrically centered split point that never moves
//  >0  Use spatial decomp with a split point that is recentered every loadBalanceFreq iterations
int loadBalanceFreq=0;

// Tell jitter to reset bias on load balance
/* 0 = No reset
   1 = Reset bias, but not gear
   2 = Reset bias and reset gear to 0
*/
enum {RESET_OFF, RESET_BIAS, RESET_BIAS_GEAR} resetOnLoadBalance;

#define LB_MODULUS (loadBalanceFreq==-1)
#define LB_SPATIAL (loadBalanceFreq>=0)

// Runtime variable loadBalanceFreq does both of these things now
//#define DECOMPOSITION 0 // Toggle spatial-decomposition (0) vs straight particle splitting (1)
//#define LOAD_BALANCE_FREQ 0 // Interations between recentering split-point for spatial decomp (0 to disable)

#define DELTA_TIME 3600*2 // Size of timestep in seconds
#define BOX_SIZE 100 // Size of simulation box in meters
 // (universe is infinitely large, 
 //  this is the region in which particles are initially scattered, 
 //  and the image output window dimensions)
#define MASS_MIN 5   // kg
#define MASS_MAX 100  // kg
#define GRAV_CONST 6.67e-11  // Nm^2/kg^2
#define MIN_RANGE 3 // Range at which gravity doesn't do anything...
// (a kludge to prevent the universe from tearing itself apart, 
//  since I don't have any repulsive forces.  Think of it
//  as a crude repulsive force that exactly counteracts gravity within
//  the given radius)

// Dimensions of the image output
#define SX 320
#define SY 320

// Turn on CSV stat tracking to stats.csv?
#define STAT_TRACKING 0

// Turn on tons of low-level debug output (particle position calculations)?
#define DEBUG_MATH 0

// Turn on writing of pt-<NODE>-<STEP>.ptcl files
#define DEBUG_WRITE_STATES 0

// Output TGA image sequences to ./img? 0=no, 1=for root only, 2=for all nodes
#define IMAGE_OUTPUT 0

// Set to zero for a new run every time, to a specific number to reproduce a run
// Can be overridden by command line setting
#define RAND_SEED 0

// Set to 1 to enable the mpidummy call and all other JITTER-related things
#define JITTER_DUMMY 1

// Spatial decomposition split point (default: center of the box) (used for spatial decomp ownership calculation)
Vector3D splitPoint = {BOX_SIZE/2,BOX_SIZE/2,BOX_SIZE/2};

MPI_Datatype particleType; // MPI data type of Particle
   
MPI_Datatype particleEntryType; // MPI data type of ParticleEntry







/**
 * Given index into particle[] i, return the owner process of that particle.
 * Depends on loadBalanceFreq setting.
 * @param i Index into the particle[] array
 * @return Owner process number (based on on load balance scheme)
 */
inline char getOwner(int i) {
	if (LB_MODULUS) return i % numProcs;
	
	Particle pt = particle[i];
	char result = ((pt.p.x < splitPoint.x)<<2 | (pt.p.y < splitPoint.y)<<1 | (pt.p.z < splitPoint.z)) % numProcs;
	return result; //return ((i<=2500)?result:2);
}

/**
 * Partitions a subarray a[left..right] on the pivot a[pivotIndex].  Used as
 * part of the median-finding algorithm.
 * @param a Array whose subset we will partition
 * @param left Leftmost index to include
 * @param right Rightmost index to include
 * @param pivotIndex Index of element to pivot upon
 * @return Resulting pivot index
 */
int partition(float* a, int left, int right, int pivotIndex) {
	int i;
	float t;
	float pivotValue = a[pivotIndex];
	t=a[pivotIndex]; a[pivotIndex]=a[right]; a[right]=t;  // swap a[pi],a[r]: Move pivot to end
	int storeIndex = left;
	for (i=left; i<right; i++) {
		if (a[i] <= pivotValue) {
			t=a[storeIndex]; a[storeIndex]=a[i]; a[i]=t; // swap
			storeIndex++;
		}
	}
	t=a[right]; a[right]=a[storeIndex]; a[storeIndex]=t;  // Move pivot to its final place
	return storeIndex;
}

/**
 * Returns the value of the kth smallest value in the subarray a[left..right].
 * Array need not be sorted.
 * @param a Array whose subset we will consider
 * @param k How many smaller numbers exist before the returned one
 * @param left Leftmost index to include
 * @param right Rightmost index to include
 * @return Value of kth smallest number
 */
float selectKth(float* a, int k, int left, int right) {
	while (1) {
		int pivotIndex = left; //select a pivot value a[pivotIndex]
		int pivotNewIndex = partition(a, left, right, pivotIndex);
		if (k == pivotNewIndex)
			return a[k];
		else if (k < pivotNewIndex)
			right = pivotNewIndex-1;
		else
			left = pivotNewIndex+1;
	}
}

/**
 * Performs spatial load balancing; called only when loadBalanceFreq >= 0.
 * Basically sets split-point to 3D median of particle cloud.
 * Optionally resets bias and gears based on value of resetOnLoadBalance.
 */
void loadBalance() {
	int i;

	if (!myRank) printf("Load balancing...");
	
	float* p = calloc(numParticles,sizeof(float));
	
	for (i=0; i<numParticles; i++) p[i] = particle[i].p.x;
	splitPoint.x = selectKth(p,numParticles/2,0,numParticles-1);
	
	for (i=0; i<numParticles; i++) p[i] = particle[i].p.y;
	splitPoint.y = selectKth(p,numParticles/2,0,numParticles-1);
	
	for (i=0; i<numParticles; i++) p[i] = particle[i].p.z;
	splitPoint.z = selectKth(p,numParticles/2,0,numParticles-1);

	if (!myRank) printf("DONE!\n");

#if JITTER_DUMMY
	if (resetOnLoadBalance == RESET_BIAS) reset_bias(10);
	if (resetOnLoadBalance == RESET_BIAS_GEAR) reset_all();
#endif

	free(p);
}

/**
 * Subtract b from a and return the result
 * @param a Left vector
 * @param b Right vector
 * @return (b-a)
 */
inline Vector3D sub(Vector3D* a, Vector3D* b) {
    Vector3D r;
    r.x = a->x - b->x;
    r.y = a->y - b->y;
    r.z = a->z - b->z;
    return r;
}

/**
 * Add b to a, storing the result in a: a += b.
 * @param a Accumulator vector
 * @param b Addend
 */
inline void accum(Vector3D* a, Vector3D b) {
    Vector3D r;
    a->x += b.x;
    a->y += b.y;
    a->z += b.z;
}

/**
 * Scalar multiply: multiply a by c and return the result.
 * @param c Factor to multiply by
 * @param a Vector to multiply
 * @return c*a
 */
inline Vector3D smul(float c, Vector3D* a) {
    Vector3D r;
    r.x = c * a->x;
    r.y = c * a->y;
    r.z = c * a->z;
    return r;
}

/**
 * Get the square of the magnitude of vector a
 * @param a Vector to get magnitude of
 * @return |a|
 */
#define mag2(a) (a).x*(a).x + (a).y*(a).y + (a).z*(a).z

/**
 * Make the given vector unit length
 * @param a Vector to normalize
 */
inline void normalize(Vector3D* a) {
	float m = mag2(*a);
	a->x /= sqrt(m);
	a->y /= sqrt(m);
	a->z /= sqrt(m);
}

/**
 * Return a random float between a and b
 * @param a Minimum return value
 * @param b Maximum return value
 * @return Random value in interval [a,b]
 */
inline float randInRange(float a, float b) {
    return ((float)rand())/RAND_MAX*(b-a)+a;
}

/**
 * Complains and dies if parameter is nonzero.  Use this to wrap MPI calls to
 * automatically verbosely abort on error.  For example:
 *   mpiWrap(MPI_Comm_size(MPI_COMM_WORLD, &numProcs));
 * @param mpiResult Return value of MPI call
 */
void mpiWrap (int mpiResult) {
	char buf[256];
	int bufSize;
	if (mpiResult) {
		MPI_Error_string(mpiResult,buf,&bufSize);
		printf("MPI Error %d: '%s'!\n",mpiResult,buf);
		fprintf(stderr,"!MPI Error %d: '%s'!\n",mpiResult,buf);
		exit(1);
	}
}

/**
 * The root node will create a random batch of particles spread out uniformly
 * over a box with size BOX_SIZE, then broadcast it.  Non-root nodes will
 * receive these particles.
 * @param randSeed The random seed to use, zero to autogenerate one
 * @returns If rank is zero, returns the random seed used, else returns -1
 */
int initParticlesRandom(int randSeed) {
    int i;
    particle = (Particle*)calloc(numParticles,sizeof(Particle));
	partSend = (ParticleEntry*)calloc(numParticles,sizeof(ParticleEntry));
	partRecv = (ParticleEntry*)calloc(numParticles,sizeof(ParticleEntry));
	if (!myRank) {
    
		if (!randSeed) randSeed = time(NULL);
		srand(randSeed);
		
		for (i=0; i<numParticles; i++) {
			particle[i].p.x = randInRange(0,BOX_SIZE);
			particle[i].p.y = randInRange(0,BOX_SIZE);
			particle[i].p.z = randInRange(0,BOX_SIZE);
	
			particle[i].v.x = particle[i].v.y = particle[i].v.z = 0.0;
	
			particle[i].mass = randInRange(MASS_MIN,MASS_MAX);
		}
		
		
	}
	
	mpiWrap(MPI_Bcast(particle,numParticles,particleType,0,MPI_COMM_WORLD));
	
	return myRank ? -1 : randSeed;
}

/**
 * Render a TGA image of the current state of particles in 2D to the filename
 * given.
 * @param fn Filename to write to (it should be a .tga)
 */
void writeImage(char* fn) {
    int i;
    Color3b* img = createImage24bit(SX,SY); // Allocate memory
	
    for (i=0; i<numParticles; i++) {
        int x = (int) (particle[i].p.x*SX/BOX_SIZE); // x-coord in image
        if (x<0 || x>=SX) continue; // skip if OOB
		
        int y = (int) (particle[i].p.y*SX/BOX_SIZE); // y-coord in image
        if (y<0 || y>=SY) continue; // skip if OOB
		
        Color3b c; // color of pixel
        int low=60; // Minimum intensity of blue pixel at minimum mass
        
		//128;
		c.r=255;
		c.g =c.b= (Color1b) (particle[i].mass*(255-low)/MASS_MAX+low);

        img[SX*y+x] = c;
    }

	// Draw split point divider
	int x,y;
	if (LB_SPATIAL) {
		x=(int)(splitPoint.x*SX/BOX_SIZE);
		for (y=0; y<SY; y+=2) img[SX*y+x].b=255;
	
		y=(int)(splitPoint.y*SY/BOX_SIZE);
		for (x=0; x<SX; x+=2) img[SX*y+x].b=255;
	} else {
		x=(int)(splitPoint.x*SX/BOX_SIZE);
		for (y=0; y<SY; y+=4) img[SX*y+x].b=192;
	
		y=(int)(splitPoint.y*SY/BOX_SIZE);
		for (x=0; x<SX; x+=4) img[SX*y+x].b=192;
	}
		

	// Actually write the targa image using SimpleGraphics
    writeTGA(fn,SX,SY,img);
    
    if (img) free(img); // Exterminate the memory

}

/**
 * Render a picture of the current particle state in 2D to a file numbered with
 * the given step number.
 * @param step Step number to use in filename
 */
void writeImageStep(int step) {
    char fn[64];
#if IMAGE_OUTPUT==2 // all nodes
    sprintf(fn,"img/IMG-%d-%04d.tga",myRank,step);
#else // root only
    sprintf(fn,"img/IMG-%04d.tga",step);
#endif
    writeImage(fn);
}

/**
 * Print a line of CSV to fpStats indicating the particle load on each machine
 * @param numOwned Number of particles this node owns
 */
void printDecompStats(int* numOwned) {
	if (!STAT_TRACKING) {
		printf("Impossible - entered printDecompStats when STAT_TRACKING was off!\n");
		exit(1);
	}

	int i;
    for (i=0;i<numProcs;i++) fprintf(fpStats,"%d%c",numOwned[i],(i<(numProcs-1))?',':'\n');
    fflush(fpStats);
    
}

/**
 * Output particle state in binary for debug reasons
 * @param step Timestep (used in generating filename)
 */
void writeParticleState(int step) {
#if DEBUG_WRITE_STATES
	char buf[64];
	sprintf(buf,"ptcl/pt-%d-%04d.ptcl",myRank,step);
	FILE* fp;
	if (!(fp = fopen(buf,"wb"))) {
		printf("Unable to open '%s' for writing.\n",buf);
		return;
	}
	fwrite(particle,sizeof(Particle),numParticles,fp);
	fclose(fp);	
#endif
}

/**
 * This is the big important function.  This is what actually runs the whole
 * simulation.
 */
void simulate() {
    int i,j,step;
	// Accelerations of each particle - only needed on this node
    Vector3D* accelV = (Vector3D*)calloc(numParticles,sizeof(Vector3D));
	int* owner = (int*)calloc(numParticles,sizeof(int));
	int* numOwned = (int*)calloc(numProcs,sizeof(int));

	// Time of timesteps after which to report the current status
    int reportInterval = timeSteps/50;
    if (reportInterval <= 0) reportInterval=1;
	
	writeParticleState(0);
	
    for (step=0; step<timeSteps; step++) {
        if (!myRank) {
			if (step%reportInterval==0) printf("Timestep %d/%d\n",step,timeSteps);
#if IMAGE_OUTPUT==1 // root node only
			writeImageStep(step);
#endif
		}
#if IMAGE_OUTPUT==2 // all nodes
		writeImageStep(step);
#endif
#if JITTER_DUMMY
		int x; mpidummy_(&x);
#endif

		if (loadBalanceFreq>0 && step % loadBalanceFreq == 0) loadBalance();

		for (i=0; i<numProcs; i++)  numOwned[i]=0;
		
        // Calculate all accelerations
        for (i=0; i<numParticles; i++) {
			owner[i] = getOwner(i);
			numOwned[owner[i]]++;
			if (owner[i] != myRank) continue;
			
			accelV[i].x=accelV[i].y=accelV[i].z=0.0;
			for (j=0; j<numParticles; j++) {
				if (i==j) continue;

                Vector3D delta = sub(&particle[j].p,&particle[i].p); // vector Pj-Pi
                float r2 = mag2(delta); // radius^2
                if (r2 < MIN_RANGE*MIN_RANGE)  continue; // Skip close particles, see comment near #define MIN_RANGE
#if DEBUG_MATH                
                if (j==50) printf("[%d,%d] (%f,%f,%f)-(%f,%f,%f)\n  D(%f,%f,%f) r2=%f \n",
                                  i,j,
                                  particle[i].p.x,particle[i].p.y,particle[i].p.z,
                                  particle[j].p.x,particle[j].p.y,particle[j].p.z,
                                  delta.x,delta.y,delta.z,r2);
#endif
                normalize(&delta); // Normalize delta vector
                float force = GRAV_CONST * particle[i].mass * particle[j].mass / r2; // Calc force
				
				Vector3D accelI = smul( force/particle[i].mass,&delta);

                accum(&accelV[i],accelI); // Calc and store acceleration

#if DEBUG_MATH
				if (j==50) printf("  Du(%f,%f,%f) F=%f A(%f,%f,%f)\n\n",
					delta.x,delta.y,delta.z,
					force,
					accelI.x,accelI.y,accelI.z);
#endif
				}
        }

#if STAT_TRACKING
		if (!myRank) printDecompStats(numOwned);
#endif

        // apply accels over dt to velocity and apply velocity over dt to position
		int c=0; // Temp var to iterate through partSend
        for (i=0; i<numParticles; i++) {
			if (owner[i] != myRank) continue;
			
            // Vi += dt*Ai
            accum(&particle[i].v,smul(DELTA_TIME,&accelV[i]));

            // Pi += dt*Vi
            accum(&particle[i].p,smul(DELTA_TIME,&particle[i].v));

			// Save to outgoing buffer    
			partSend[c].i=i;
			partSend[c].pt=particle[i];
			c++;
			
		}
		
		// Exchange particles
		for (i=0; i<numProcs; i++) {
			if (myRank == i) {
				mpiWrap(MPI_Bcast(partSend,numOwned[i],particleEntryType,i,MPI_COMM_WORLD));
				//printf("[%d] Bcast OUT, nO[i=%d]=%d  buf[0]:i=%d x=%f ------------\n",myRank,i,numOwned[i],partSend[0].i,partSend[0].pt.p.x);
			} else {
				//printf("[%d] Bcast  IN, nO[i=%d]=%d\n",myRank,i,numOwned[i]);
				mpiWrap(MPI_Bcast(partRecv,numOwned[i],particleEntryType,i,MPI_COMM_WORLD));
				//printf("[%d] Bcast  IN, nO[i=%d]=%d  buf[0]:i=%d x=%f\n",myRank,i,numOwned[i],partRecv[0].i,partRecv[0].pt.p.x);
				
				// Spray this received data into our main array
				for (j=0; j<numOwned[i]; j++) {
					particle[partRecv[j].i] = partRecv[j].pt;
				}
			}
		}
		
		writeParticleState(step+1);
	}
	
	free(accelV);
	free(owner);
	free(numOwned);

}

/**
 * Free up particle memory
 */
void cleanUp() {
	if (particle) free(particle);
	if (partSend) free(partSend);
	if (partRecv) free(partRecv);
}

/**
 * The main function...you know the drill.  See giant help banner for program
 * syntax.
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 */
int main(int argc, char** argv) {

	setbuf(stdout,NULL); // Disable stdout buffering for instant knowledge
	
    if (argc < 3 ||
        sscanf(argv[1],"%d",&numParticles)!=1 || numParticles<=0 ||
        sscanf(argv[2],"%d",&timeSteps)!=1 || timeSteps<=0
        ) {
        printf(
            "MPI N-Body Solver by Tyler Bletsch\n"
            "\n"
            "Syntax: \n"
            "  %s <NumParticles> <TimeSteps> [LoadBalanceFreq] [RandSeed] [ResetOnBalance]\n"
            "\n"
            "Where:\n"
            "  NumParticles: Number of particles to simulate\n"
            "  TimeSteps: Number of timesteps to simulate\n"
            "  LoadBalanceFreq: Frequency to load balance:\n"
            "    -1  = Use perfect modulus balancing\n"
            "     0  = Never load balance\n"
            "     1+ = Load balance every LoadBalanceFreq iterations\n"
            "  RandSeed: Random seed for particle generation, 0 for current time\n"
            "  ResetOnBalance:\n"
            "    0=Don't do any JITTER resetting on load balance\n"
            "    1=On load balance, reset JITTER bias, but not gear\n"
            "    2=On load balance, reset JITTER bias and change gear to zero\n"
            "\n"
            ,argv[0]);
        exit(1);
    }
	
	mpiWrap(MPI_Init ( &argc, &argv ));
	
	mpiWrap(MPI_Comm_size ( MPI_COMM_WORLD, &numProcs ));
	
	mpiWrap(MPI_Comm_rank ( MPI_COMM_WORLD, &myRank ));
	
	// Register Particle mpi type
	mpiWrap(MPI_Type_contiguous( sizeof(Particle)/sizeof(float), MPI_FLOAT, &particleType ));
	mpiWrap(MPI_Type_commit ( &particleType ));
	
	// Register ParticleEntry mpi type
	int blocks[] = {1,1};
	MPI_Aint displacements[] = {0,sizeof(int)};
	MPI_Datatype dataTypes[] = {MPI_INT, particleType};
	mpiWrap(MPI_Type_struct(2,blocks,displacements,dataTypes,&particleEntryType));
	mpiWrap(MPI_Type_commit ( &particleEntryType ));
	
	if (argc>=4) {
		if (sscanf(argv[3],"%ld",&loadBalanceFreq) != 1) {
			printf("Failed to parse load balance setting\n");
			exit(1);
		}
	}
	
	int randSeed = RAND_SEED;
	if (!myRank && argc>=5) {
		if (sscanf(argv[4],"%ld",&randSeed) != 1) {
			printf("Failed to get random seed from command line\n");
			exit(1);
		}
	}

	//resetOnLoadBalance
	if (argc>=6) {
		if (sscanf(argv[5],"%d",&resetOnLoadBalance) != 1) {
			printf("Failed to get reset on load balance setting\n");
			exit(1);
		}
	}
	
	if (!myRank) {
    	printf("Options:\n");
		printf("  %-30s : %d.\n","Number of nodes",numProcs);
		printf("  %-30s : %s.\n","Image output",
			IMAGE_OUTPUT?(IMAGE_OUTPUT==1?"ENABLED FOR ROOT":"ENALBED FOR ALL"):"DISABLED");
		printf("  %-30s : %s.\n","Stat tracking",STAT_TRACKING?"ENABLED":"DISABLED");
		printf("  %-30s : %d.  (-1=perfect, 0=never, >0=regular)\n","Load balance frequency",loadBalanceFreq);
		printf("  %-30s : %s.\n","Decomposition",LB_MODULUS?"MODULUS":"SPATIAL");
		printf("  %-30s : %s.\n","Math debugging output",DEBUG_MATH?"ENABLED":"DISABLED");
		printf("  %-30s : %s.\n","Writing of particle states",DEBUG_WRITE_STATES?"ENABLED":"DISABLED");
		printf("  %-30s : %s.\n","Jitter dummy call",JITTER_DUMMY?"ENABLED":"DISABLED");
		printf("  %-30s : %d. (0=off,1=bias,2=all)\n","Reset on load balance",resetOnLoadBalance);
    }

    randSeed = initParticlesRandom(randSeed);

#if STAT_TRACKING
    fpStats=fopen("stats.csv","w");
    if (!fpStats) {
        printf("Unable to open stats for writing!\n");
        exit(1);
    }
    fprintf(fpStats,"\"PARTICLES=\",%d,,\"RANDSEED=\",%d,,\"PARTICLE_MEM=\",%d\n\n",numParticles,randSeed,numParticles*sizeof(Particle));
#endif

    if (!myRank) printf("Particles occupy %d bytes, RandSeed=%d (%s).\n",numParticles*sizeof(Particle),randSeed,RAND_SEED?"chosen":"original");

	// Call the big function.  ALl the work happens here, folks!
    simulate();
    
#if STAT_TRACKING
    fclose(fpStats);
#endif

    cleanUp();
    
    mpiWrap(MPI_Finalize());
    
    return 0;

}


