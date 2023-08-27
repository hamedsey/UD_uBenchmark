/*
 * Written by Hamed Seyedroudbari (Arm Research Intern - Summer 2022)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <random>
#include <iostream>
#include <array>
#include <algorithm>

#include <infiniband/verbs.h>
#include <linux/types.h>  //for __be32 type
#include "ClientRDMAConnection.cc"
#include <vector>
#include <list>
#include <sys/mman.h>
#include <pthread.h> 
#include "rdma_uc.cc"

//#define active_thread_num 12+1  		//n loading threads,1 meas. thread 
#define debug 0
#define INTERVAL 10000000 		//RPS MEAS INTERVAL
#define SYNC_INTERVAL 1000000 	//RPS MEAS INTERVAL

#define RR 0			//enables round robin request distribution per thread
#define RANDQP 0				//enables random request distribution per thread
#define MEAS_RAND_NUM_GEN_LAT 0	//enables measuring latency of random number generator 
#define MEAS_GEN_LAT 0	//enables measuring latency of random number generator 
#define ENABLE_SERV_TIME 0
#define SERVICE_TIME_SIZE 0x8000
#define SEND_SERVICE_TIME 250
#define REORDER 1 //set to zero if you want all traffic to go to a single server core (AND turnoff assert on server)

#define RDMA_WRITE_TO_BUF_WITH_EVERY_SEND 0
#define MEASURE_LATENCIES_ON_CLIENT 1

enum {
	FIXED = 0,
	NORMAL = 1,
	UNIFORM = 2,
	EXPONENTIAL = 3,
    BIMODAL = 4,
	FB =  5,
	PC = 6,
	SQ = 7,
	NC = 8,
	EXP = 9,
};

/*
#include <memory>
#include <atomic>
template<typename T>
class lock_free_queue
{
private:
    struct node
    {
        std::shared_ptr<T> data;
        std::atomic<node*> next;
        node(T const& data_):
            data(std::make_shared<T>(data_))
        {}
    };
    std::atomic<node*> head;
    std::atomic<node*> tail;
public:
    void push(T const& data)
    {
        std::atomic<node*> const new_node=new node(data);
        node* old_tail = tail.load();
        while(!old_tail->next.compare_exchange_weak(nullptr, new_node)){
          node* old_tail = tail.load();
        }
        tail.compare_exchange_weak(old_tail, new_node);
    }
    std::shared_ptr<T> pop()
    {
        node* old_head=head.load();
        while(old_head &&
            !head.compare_exchange_weak(old_head,old_head->next)){
            old_head=head.load();
        }
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
};
*/


pthread_barrier_t barrier; 
pthread_barrierattr_t attr;
int ret; 

pthread_barrier_t barrier2; 
pthread_barrierattr_t attr2;
int ret2; 

int remote_qp0;
int distribution_mode = EXPONENTIAL;
double *rps;
int window_size = 1;
uint16_t active_thread_num = 1;
char* output_dir;
char *ib_devname_in;
int gidx_in;
int remote_qp0_in;
char* servername;
/*volatile*/ bool terminate_load = false;
/*volatile*/ bool terminate_all = false;

int SERVER_THREADS = 1;
uint64_t goalLoad = 0;
uint16_t mean = 1000;
uint16_t numberOfPriorities = 0; //not used
uint64_t tcp_port = 20002;

//#include "readerwriterqueue.h"
//#include "atomicops.h"

//using namespace moodycamel;

//ReaderWriterQueue<uint64_t> q;       // Reserve space for at least 100 elements up front


//for bimodal service time distribution
int bimodal_ratio = -1;
int long_query_percent = -1;

uint32_t * all_rcnts;
uint32_t * all_scnts;

uint64_t **sendDistribution;
uint64_t **recvDistribution;

double totalRPS = 0;
bool warmUpFinished = false;
bool warmUpDone = false;

#if MEASURE_LATENCIES_ON_CLIENT
//vector<vector<vector<uint64_t> > > latencies;
uint64_t ***latencies;
#endif

uint64_t runtime = 10; //this value is in seconds

inline double now() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

struct thread_data{
	RDMAConnection *conn;
	int id;
	uint64_t** latencyArr;
	uint64_t* sends;
	uint64_t* receives;
};

/*
static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	printf("  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
	printf("  -s, --size=<size>      size of message to exchange (default 2048)\n");
	printf("  -r, --rx-depth=<dep>   number of receives to post at a time (default 500)\n");
	printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
	printf("  -b, --debug            Enable debug prints\n");
        printf("  -l, --sl=<SL>          send messages with service level <SL> (default 0)\n");
	printf("  -e, --events           sleep on CQ events (default poll)\n");
	printf("  -g, --gid-idx=<gid index> local port gid index\n");
	printf("  -c, --chk              validate received buffer\n");
}
*/


inline void my_sleep(uint64_t n) {
	//if(thread_num ==0) printf("mysleep = %d \n",n);
	struct timespec ttime,curtime;
	clock_gettime(CLOCK_MONOTONIC,&ttime);
	
	while(1){
		clock_gettime(CLOCK_MONOTONIC,&curtime);
		uint64_t elapsed = (curtime.tv_sec-ttime.tv_sec )/1e-9 + (curtime.tv_nsec-ttime.tv_nsec);

		if (elapsed >= n) {
			//printf("elapsed = %d \n",elapsed);
			break;
		}
	}

	return;
}


double rand_gen() {
   // return a uniformly distributed random value
   return ( (double)(rand()) + 1. )/( (double)(RAND_MAX) + 1. );
}

uint8_t gen_priority(uint8_t *priorityID) {

	static uint16_t index = 0;
	uint8_t result = priorityID[index];
	//printf("index = %lu, OR  = %lu \n",index,SERVICE_TIME_SIZE | index);
	if( (SERVICE_TIME_SIZE | index) == 0xFFFF) {
		//printf("worked!, index = %lu \n",index);
		index = 0;
	}
	else {
		//printf("index = %lu \n",index);
		index++;
	}
	return result;
}


uint64_t gen_latency(int mean, int mode, int isMeasThread, uint64_t *serviceTime) {
    
	if(isMeasThread == 1) return mean;
	//else if (mode == FIXED) return mean;
	else {
		static uint16_t index = 0;
		uint64_t result = serviceTime[index];
		//printf("index = %lu, OR  = %lu \n",index,SERVICE_TIME_SIZE | index);
		if( (SERVICE_TIME_SIZE | index) == 0xFFFF) {
			//printf("worked!, index = %lu \n",index);
			index = 0;
		}
		else {
			//printf("index = %lu \n",index);
			index++;
		}

		//if(result != 1000) printf("index = %lu, wrong value = %d \n", index, result);
		#if MEAS_GEN_LAT 
			printf("gen lat = %d \n",result); 
		#endif
		if (result < 0) result = 0;
		return result;
	}
	/*
	if (mode == FIXED) {
		return mean;
	} else if (mode ==NORMAL) {
		std::random_device rd{};
		std::mt19937 gen{rd()};
		double stddev  = mean/3;
		std::normal_distribution<double> normal{mean, stddev};
		int result = normal(gen);
		if (result < 0) result = 0;
		return result;
	} else if (mode == UNIFORM) {
		return ( (double)(rand()) + 1. )/( (double)(RAND_MAX) + 1. )*mean*2;
	} else if (mode == EXPONENTIAL) {
		//std::random_device rd{};
		//std::mt19937 gen{rd()};
		//std::exponential_distribution<double> exp{1/(double)mean};	

        static int index = 0;
        int result = serviceTime[index];
        if(index = SERVICE_TIME_SIZE-1) index = 0;	
        else index++;

        #if MEAS_GEN_LAT 
            printf("gen lat = %d \n",result); 
        #endif
		if (result < 0) result = 0;
		return result;
	} else if (mode == BIMODAL) {
        //bimodal_ratios = [2, 5, 10, 25, 50, 100]
        //percents = [0.01, 0.05, 0.1, .25, .5]

        std::random_device rd;
        std::mt19937 gen(rd());
        std::discrete_distribution<> d({100-long_query_percent, long_query_percent});
        int result = d(gen);
        //printf("result = %d \n",result);
		if (result == 0) return mean;
		else return mean*bimodal_ratio;
	} else {
		return mean;
	}
	*/
}

uint64_t gen_arrival_time(uint64_t *arrivalSleepTime) {
    
	static uint16_t index = 0;
	uint64_t result = arrivalSleepTime[index];
	//printf("index = %lu, OR  = %lu \n",index,SERVICE_TIME_SIZE | index);
	if( (SERVICE_TIME_SIZE | index) == 0xFFFF) {
		//printf("worked!, index = %lu \n",index);
		index = 0;
	}
	else {
		//printf("index = %lu \n",index);
		index++;
	}
	return result;
}


uint16_t genRandDestQP(uint8_t thread_num) {          

	#if MEAS_RAND_NUM_GEN_LAT
		struct timespec randStart, randEnd;
		clock_gettime(CLOCK_MONOTONIC, &randStart);
	#endif

	//UNCOMMENT ONE OF 4 RANDOM DISTRIBTIONS
	
	//(1)
	/*
	//period 2^96-1unsigned long t;
	static unsigned long x=123456789, y=362436069, z=521288629, t;

	x ^= x << (16-thread_num);
	x ^= x >> 5;
	x ^= x << 1;

	t = x;
	x = y;
	y = z;
	z = t ^ x ^ y;

	//z = thread_num;
	//uint8_t ret = ((z >> 2) & 1u) & ((z >> 3) & 1u) ? (z & 3u) :(z & 0xf);
	uint8_t ret = z%12;
	*/

	//(2)
	/*
	static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
	unsigned int b;
	b  = ((z1 << 6) ^ z1) >> (13-thread_num);
	z1 = ((z1 & 4294967294U) << 18) ^ b;
	b  = ((z2 << 2) ^ z2) >> 27; 
	z2 = ((z2 & 4294967288U) << 2) ^ b;
	b  = ((z3 << 13) ^ z3) >> 21;
	z3 = ((z3 & 4294967280U) << 7) ^ b;
	b  = ((z4 << 3) ^ z4) >> 12;
	z4 = ((z4 & 4294967168U) << 13) ^ b;

	unsigned int z = (z1 ^ z2 ^ z3 ^ z4);
	//uint8_t ret = ((z >> 2) & 1u) & ((z >> 3) & 1u) ? (z & 3u) :(z & 0xf);
	uint8_t ret = z%12;
	*/

	//(3)
	
	static __uint64_t g_lehmer32_state = 0x60bee2bee120fc15;
	g_lehmer32_state *= 0xe4dd58b5;
	uint16_t ret = (g_lehmer32_state >> (32-2*thread_num)) % SERVER_THREADS;
	

	//(4)
	/*
	static uint64_t shuffle_table[4] = {1, 2, 3, 4};
	uint64_t s1 = shuffle_table[0];
	uint64_t s0 = shuffle_table[1];
	uint64_t result = s0 + s1;
	shuffle_table[0] = s0;
	s1 ^= s1 << 23;
	shuffle_table[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
	uint8_t ret = result % 12;
	*/

	#if MEAS_RAND_NUM_GEN_LAT
		clock_gettime(CLOCK_MONOTONIC, &randEnd);
		printf("rand-time = %f \n",(randEnd.tv_sec-randStart.tv_sec)/1e-9 +(randEnd.tv_nsec-randStart.tv_nsec));
	#endif

	return ret;
}

void* client_send(void* x) {

	//struct thread_data *tdata = (struct thread_data*) x;
	int thread_num = ((struct thread_data*) x)->id;
	uint64_t *sends = ((struct thread_data*) x)->sends;

	//printf("thread id = %d \n", thread_num);
	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset
    CPU_SET(thread_num+24, &cpuset);  //set CPU 2 on cpuset
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	RDMAConnection *conn = ((struct thread_data*) x)->conn;

	uint64_t *serviceTime = (uint64_t *)malloc(SERVICE_TIME_SIZE*sizeof(uint64_t));
	uint8_t *priorityID = (uint8_t *)malloc(SERVICE_TIME_SIZE*sizeof(uint64_t));


	if(distribution_mode == FIXED) {
		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			serviceTime[i] = mean;
			priorityID[i] = 0;
		}
	}
	else if(distribution_mode == EXPONENTIAL) {
		std::random_device rd{};
		std::mt19937 gen{rd()};
		std::exponential_distribution<double> exp{1/(double)mean};
		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) serviceTime[i] = exp(gen);
	}
	else if(distribution_mode == BIMODAL) {
		std::random_device rd{};
		std::mt19937 gen{rd()};
		std::discrete_distribution<> bm({double(100-long_query_percent), double(long_query_percent)});
		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			uint8_t result = bm(gen);
			//printf("result = %d \n",result);
			if (result == 0) serviceTime[i] = mean;
			else serviceTime[i] = mean*bimodal_ratio;
		
			priorityID[i] = result;
		}
	}
	else if(distribution_mode == FB) {
		std::random_device rd{};
		std::mt19937 gen{rd()};
		//std::discrete_distribution<> bm;

		/*
		if(numberOfPriorities == 1) std::discrete_distribution<> bm({double(100)});
		else if (numberOfPriorities == 2) std::discrete_distribution<> bm({double(50), double(50)});
		else if (numberOfPriorities == 3) std::discrete_distribution<> bm({double(34), double(33), double(33)});
		else if (numberOfPriorities == 4) std::discrete_distribution<> bm({double(25), double(25), double(25), double(25)});
		else if (numberOfPriorities == 5) std::discrete_distribution<> bm({double(20), double(20), double(20), double(20),double(20)});
		else if (numberOfPriorities ==10) std::discrete_distribution<> bm({double(10), double(10), double(10), double(10),double(10), double(10), double(10), double(10), double(10),double(10)});
		else if (numberOfPriorities ==20) std::discrete_distribution<> bm({double(5), double(5), double(5), double(5),double(5), double(5), double(5), double(5), double(5),double(5),double(5), double(5), double(5), double(5),double(5), double(5), double(5), double(5), double(5),double(5)});
		else if (numberOfPriorities ==50) std::discrete_distribution<> bm({double(2), double(2), double(2), double(2),double(2), double(2), double(2), double(2), double(2),double(2)
										,double(2), double(2), double(2), double(2),double(2), double(2), double(2), double(2), double(2),double(2)
										,double(2), double(2), double(2), double(2),double(2), double(2), double(2), double(2), double(2),double(2)
										,double(2), double(2), double(2), double(2),double(2), double(2), double(2), double(2), double(2),double(2)
										,double(2), double(2), double(2), double(2),double(2), double(2), double(2), double(2), double(2),double(2)});
		else if (numberOfPriorities==100) std::discrete_distribution<> bm({double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)
										,double(1), double(1), double(1), double(1),double(1), double(1), double(1), double(1), double(1),double(1)});
		*/
 
    	std::uniform_int_distribution<uint64_t> bm(0, numberOfPriorities-1);


		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			//printf("result = %d \n",result);
			serviceTime[i] = mean;		
			priorityID[i] = bm(gen);
		}

		/*
		for (int i = 0; i < SERVICE_TIME_SIZE; ++i) {
			printf("%llu ",priorityID[i]);
			if ((i + 1) % 20 == 0) // Print 20 numbers per line for readability
				printf("\n");
		}
		*/
	}
	else if(distribution_mode == PC) {
		std::random_device rd{};
		std::mt19937 gen{rd()};
		std::discrete_distribution<> bm({95, 5});
		
		uint16_t numQueuesGettingMoreLoad = numberOfPriorities/5; //8/5;
		uint16_t twentyPercent = 0;
		uint16_t eightyPercent = numQueuesGettingMoreLoad;

		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			//printf("result = %d \n",result);
			serviceTime[i] = mean;	
			if(bm(gen) == 0) {
				priorityID[i] = twentyPercent;//8*twentyPercent;
				if(twentyPercent == numQueuesGettingMoreLoad-1) twentyPercent = 0;
				else twentyPercent++;
			}
			else {
				priorityID[i] = eightyPercent;//8*eightyPercent;
				if(eightyPercent == numberOfPriorities-1/*7*/) eightyPercent = numQueuesGettingMoreLoad;
				else eightyPercent++;
			}
		}
	}
	else if(distribution_mode == SQ) {
		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			//printf("result = %d \n",result);
			serviceTime[i] = mean;		
			priorityID[i] = numberOfPriorities-1;
		}
	}
	else if(distribution_mode == NC) {
		//traffic passes through 20 queues all the time and through the rest with a probability of 5%
		std::random_device rd{};
		std::mt19937 gen{rd()};
		std::discrete_distribution<> bm({95, 5});
		
		uint16_t numQueuesGettingMoreLoad = 20;
		//uint16_t numQueuesGettingLessLoad = numberOfPriorities-numQueuesGettingMoreLoad;

		uint16_t twentyPercent = 0;
		uint16_t eightyPercent = numQueuesGettingMoreLoad;

		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			//printf("result = %d \n",result);
			serviceTime[i] = mean;	
			if(bm(gen) == 0) {
				priorityID[i] = twentyPercent;
				if(twentyPercent == numQueuesGettingMoreLoad-1) twentyPercent = 0;
				else twentyPercent++;
			}
			else {
				priorityID[i] = eightyPercent;
				if(eightyPercent == numberOfPriorities-1) eightyPercent = numQueuesGettingMoreLoad;
				else eightyPercent++;
			}
		}
	}
	else if(distribution_mode == EXP) {

		//generating 10000 item array of discrete exponentially distributed priorities
		const uint64_t arrayLength = 10000;
        std::array<uint64_t, arrayLength> arr;
		std::random_device rd{};
		std::mt19937 gen{rd()};
		double meanPriorities = numberOfPriorities/2;//4;
		//double lambda = 100; // Exponential distribution parameter
		std::exponential_distribution<double> exp{1/(double)meanPriorities};
		for (int i = 0; i < arr.size(); ++i) {
			// Generate a random number from the exponential distribution
			double randomNumber = exp(gen);

			// Scale the random number to the range [0, 255]
			uint64_t scaledNumber = static_cast<uint64_t>(randomNumber) % numberOfPriorities;

			arr[i] = (numberOfPriorities-1) - scaledNumber;
		}

		// Sort the array in ascending order
		std::sort(arr.begin(), arr.end());
		/*
		uint64_t count = 0;
		for(int i = 0; i < numberOfPriorities; i++) {
			count = 0;
			for(int j = 0; j < 10000; j++) {
				if(i == arr[j]) count++;
			}
			printf("i = %llu, count = %llu \n", i , count);
		}

		for (int i = 0; i < arr.size(); ++i) {
			std::cout << arr[i] << " ";
			if ((i + 1) % 20 == 0) // Print 20 numbers per line for readability
				std::cout << std::endl;
		}
		*/
    	std::uniform_int_distribution<uint64_t> bm(0, arrayLength-1);


		for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
			//printf("result = %d \n",result);
			serviceTime[i] = mean;		
			priorityID[i] = arr[bm(gen)];
		}
		/*
		for (int i = 0; i < SERVICE_TIME_SIZE; ++i) {
			printf("%llu ",priorityID[i]);
			if ((i + 1) % 20 == 0) // Print 20 numbers per line for readability
				printf("\n");
		}
		*/
	}


	uint64_t singleThreadWait = 1000000000/goalLoad;
	uint64_t avg_inter_arr_ns = active_thread_num*singleThreadWait;

	//if(thread_num == 6) avg_inter_arr_ns = 10000;

	uint64_t *arrivalSleepTime = (uint64_t *)malloc(SERVICE_TIME_SIZE*sizeof(uint64_t));
	std::random_device rd{};
	std::mt19937 gen{rd()};
	std::exponential_distribution<double> exp{1/((double)avg_inter_arr_ns-SEND_SERVICE_TIME)};
	for(uint64_t i = 0; i < SERVICE_TIME_SIZE; i++) {
		arrivalSleepTime[i] = exp(gen);
		//printf("arrivalSleepTime = %llu \n", arrivalSleepTime[i]);
	}

	if (thread_num == 0) {
		printf("active thread num = %d \n", active_thread_num);
		printf("goalLoad = %llu \n", goalLoad);
		printf("single thread wait = %llu \n", singleThreadWait);
		printf("avg_inter_arr_ns = %llu \n", avg_inter_arr_ns);
		printf("QPN = %llu \n", conn->ctx->qp->qp_num);
	}
	

	uint16_t i = 0;
	uint64_t number;

	int success;
	//bool stop = false;

	struct timespec ttime,curtime;
	clock_gettime(CLOCK_MONOTONIC,&ttime);
	uint64_t arrival_wait_time = 0;
	uint64_t outs_send = 0;
	uint8_t sequence_number = 0;
	bool here = false;
	while(terminate_all == false)
	{
		if(terminate_load == true) break;
		//for (int i = 0; i < window_size; i++) {
		//for (int j = 0; j < 10000000; j++) {
			//int i = j % window_size;
			//printf("inside while loop \n");

			//uncomment if exceeding window size
			
			if(here == true) {
				if(conn->sentCount - conn->scnt >= conn->rx_depth) {
					//printf("true -> true \n");
					here = true;
					continue;
				}
				//else printf("no \n");
			}
			else {
				if(conn->sentCount - conn->scnt >= conn->rx_depth) {
					//printf("false -> true \n");
					here = true;
					continue;
				}
				//else printf("no \n");
			}
			

			uint64_t req_lat = gen_latency(mean, distribution_mode,0, serviceTime);
			uint8_t priority = gen_priority(priorityID);

			req_lat = req_lat >> 4;
            #if MEAS_GEN_LAT 
                printf("lat = %d \n",req_lat); 
            #endif
			uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
			uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;f
		    //printf("sleep_int_lower = %lu, sleep_int_upper = %lu, sleep_time = %lu \n", lat_lower, lat_upper, req_lat);
			#if debug 
				//printf("lower %d; upper %d\n", lat_lower, lat_upper);
			#endif

			conn->buf_send[i][1] = lat_lower;
			conn->buf_send[i][0] = lat_upper;
			conn->buf_send[i][2] = sequence_number;
			conn->buf_send[i][3] = 255;
			conn->buf_send[i][10] = priority;
			if(sequence_number == 255) sequence_number = 0;
			else sequence_number++;
			if(warmUpFinished == true) {
				sends[priority]++;
				warmUpDone = true;
			}

			/*
					while(1) {
						if(terminate_load == true) break;
						
						if(conn->souts >= conn->rx_depth) continue;
						else break;
					}
			*/
			//conn->buf_send[i][3] = (conn->ctx->qp->qp_num & 0xFF0000) >> 16;
			//conn->buf_send[i][4] = (conn->ctx->qp->qp_num & 0x00FF00) >> 8;
			//conn->buf_send[i][5] = (conn->ctx->qp->qp_num & 0x0000FF);

			//if(conn->souts - conn->rcnt < window_size) {

				//bool succeeded = q.try_dequeue(number);  // Returns false if the queue was empty
				//assert(succeeded);
				//printf("dequeue succeeded = %d \n", succeeded);

				//if(succeeded == false) {
					//printf("went with i \n");
					//if(stop == false) 

				clock_gettime(CLOCK_MONOTONIC,&curtime);
				uint64_t elapsed = (curtime.tv_sec-ttime.tv_sec )/1e-9 + (curtime.tv_nsec-ttime.tv_nsec);
				//if (elapsed + SEND_SERVICE_TIME < arrival_wait_time) my_sleep(arrival_wait_time - elapsed - SEND_SERVICE_TIME);
				if (elapsed < arrival_wait_time) my_sleep(arrival_wait_time - elapsed);	

				#if REORDER 
					success = conn->pp_post_send(conn->ctx, remote_qp0+priority, conn->size, i);
				#else 
					success = conn->pp_post_send(conn->ctx, conn->dest_qpn, conn->size, i);
				#endif
				
				//}
				//else {
					//printf("dequeued number \n");
				//	success = conn->pp_post_send(conn->ctx, /*remote_qp0*/ conn->dest_qpn, conn->size, number);
				//}

				clock_gettime(CLOCK_MONOTONIC,&ttime);
	
				
				if (outs_send < conn->sentCount - conn->rcnt) {
					outs_send = conn->sentCount - conn->rcnt;
				}


			//printf("elapsed = %d \n",elapsed);

				arrival_wait_time = gen_arrival_time(arrivalSleepTime);


				if (success == EINVAL) printf("Invalid value provided in wr \n");
				else if (success == ENOMEM)	printf("T%d - Send Queue is full or not enough resources to complete this operation 1, souts = %d \n", thread_num, conn->sentCount - conn->scnt);
				else if (success == EFAULT) printf("Invalid value provided in qp \n");
				else if (success != 0) {
					printf("success = %d, \n",success);
					fprintf(stderr, "Couldn't post send 3 \n");
					//return 1;
				}
				else {
					conn->sentCount++;
					++conn->souts;
					//if(succeeded == false && stop == false) {
						if(i == window_size - 1) {
							//printf("reached max window size \n");
							i = 0;
							//stop = true;
						}
						else i++;
					//}
					/*
					if(conn->souts - conn->rcnt < window_size) {
						printf("exceeded window size. souts - rcnt = %llu \n",conn->souts - conn->rcnt);
					}
					*/
					#if debug 
						//printf("send posted... souts = %d, \n",conn->souts);
					#endif

				}

				#if RDMA_WRITE_TO_BUF_WITH_EVERY_SEND
					*(res.buf) = htonll(64);
					post_send(&res, IBV_WR_RDMA_WRITE);
					poll_completion(&res);
				#endif

				#if RR
					conn->offset = ((SERVER_THREADS-1) & (conn->offset+1));
					conn->dest_qpn = remote_qp0+conn->offset;
				#endif	

				#if RANDQP
					conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
				#endif
				
				//my_sleep(10, thread_num);

				//printf("arrival_wait_time = %lu \n",arrival_wait_time);
			//}
		//}
	}
	printf("send thread exits while loop \n");
	//printf("T%d send thread outstanding sends = %llu \n", thread_num, outs_send);

	//insert barrier to send last pkt after receive threads have drained.
	//ret2 = pthread_barrier_wait(&barrier2);
}

void* client_threadfunc(void* x) {

	struct thread_data *tdata = (struct thread_data*) x;
	int thread_num = tdata->id;
	uint64_t ** latencyArr = tdata->latencyArr;
	uint64_t *receives = tdata->receives;

	//printf("thread id = %llu \n", thread_num);

	if(thread_num == 0) {
		tdata->conn = new RDMAConnection(thread_num,0,ib_devname_in,gidx_in,remote_qp0_in,servername);
		remote_qp0 = rem_dest->qpn;
	}
	else tdata->conn = new RDMAConnection(thread_num,0,ib_devname_in,gidx_in,remote_qp0_in,servername);

	//printf("after connection = %d \n", thread_num);

	RDMAConnection *conn = tdata->conn;
	//hamed: may have to change window size of each thread... window_size = conn->bufs_num/active_thread_num;

	conn->offset = thread_num%SERVER_THREADS;
	printf("thread_num = %d, offset = %d, rx_depth = %d \n", thread_num, conn->offset, conn->rx_depth);

	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset
    CPU_SET(thread_num/*+2*//*12*/, &cpuset);  //set CPU 2 on cpuset
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	sleep(1);
	ret = pthread_barrier_wait(&barrier);
	conn->dest_qpn = remote_qp0+conn->offset;

	struct timeval start, end;

    printf("T%d - remote_qp0 = 0x%06x , %d, dest_qpn = 0x%06x , %d \n",thread_num,remote_qp0,remote_qp0,conn->dest_qpn,conn->dest_qpn);


	//spawn send thread
	//struct thread_data* t_data = tdata;
	pthread_t pt;
	//t_data.conn = conn;  
	//t_data.id = thread_num;
	int ret = pthread_create(&pt, NULL, client_send, (void *)tdata);
	assert(ret == 0);

	    //ret = pthread_barrier_wait(&barrier);

        //sleep(1);
        //printf("T%d started \n",thread_num);
        //if(thread_num != 5) sleep(10);
		const int num_bufs = conn->bufs_num;

		for (unsigned int r = 0; r < window_size; ++r) {
			if(!conn->pp_post_recv(conn->ctx, r+num_bufs, false)) 
				++conn->routs;
		}
		if (conn->routs != window_size) fprintf(stderr,"Couldn't post initial receive (%d)\n",conn->routs);
			
		#if debug
			printf("T%d - remote_qp0 = %d \n",thread_num,remote_qp0);
		#endif


		//while (conn->rcnt < conn->iters || conn->scnt < conn->iters) {
		uint64_t outs_send = 0;
		uint8_t priorityID = 0;
		uint64_t latency = 0;
		uint32_t ingressTS = 0;
		uint32_t egressTS = 0;
		uint16_t bufID = 0;
		uint16_t z;
		bool succeeded;

		double prev_clock;	
		//uint64_t skipFirstReq = (goalLoad*10)/active_thread_num;
		bool initialPhase = true;

		while(terminate_load == false) {
			//if (terminate_load == true) break;

			struct ibv_wc wc[num_bufs*2];
			int ne, i;

			//do {
				ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs, wc);
				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
				}

				#if debug 
					//printf("thread_num %d polling \n",thread_num);
				#endif

			//} while (!conn->use_event && ne < 1);

			for (i = 0; i < ne; ++i) {
				if (wc[i].status != IBV_WC_SUCCESS) {
					fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(wc[i].status),
						wc[i].status, (int) wc[i].wr_id);
				}

				uint64_t a = (int) wc[i].wr_id;
				switch (a) {
					case 0 ... num_bufs-1: // SEND_WRID
						++conn->scnt;
						
						if (outs_send < conn->scnt - conn->rcnt) {
							outs_send = conn->scnt - conn->rcnt;
						}
						
						--conn->souts;
						#if debug
							printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
						#endif
						break;

					case num_bufs ... 2*num_bufs-1:
						++conn->rcnt;
						--conn->routs;

						//if(conn->rcnt == 1) printf("Completed test pkt! \n");
						#if debug
							printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
						#endif
					

						if(initialPhase == true && warmUpDone == true){
							conn->rcnt = 0;
							initialPhase = false;
							prev_clock = now();
									
							if (gettimeofday(&start, NULL)) {
								perror("gettimeofday");
							}	

						}

						//if(conn->rcnt > conn->iters - INTERVAL/1000000 && conn->rcnt % 1 == 0) printf("T%d - rcnt = %d, scnt = %d \n",thread_num,conn->rcnt,conn->scnt);
						bufID = a-num_bufs;
						priorityID = (uint8_t)conn->buf_recv[bufID][50];
						
						//for(z = 42; z <= 49; z++) printf("%x ",(uint8_t)conn->buf_recv[bufID][z]);
						//printf("\n");

						egressTS = ((uint8_t)conn->buf_recv[bufID][42] << 24) + ((uint8_t)conn->buf_recv[bufID][43] << 16) + ((uint8_t)conn->buf_recv[bufID][44] << 8) + ((uint8_t)conn->buf_recv[bufID][45]);
						ingressTS = ((uint8_t)conn->buf_recv[bufID][46] << 24) + ((uint8_t)conn->buf_recv[bufID][47] << 16) + ((uint8_t)conn->buf_recv[bufID][48] << 8) + ((uint8_t)conn->buf_recv[bufID][49]);

						//egressTS = (unsigned long *)strtoul(conn->buf_recv[bufID] + 46, NULL, 0);
						
						if(egressTS > ingressTS) latency = egressTS - ingressTS;
						else latency = 4294967295 + (egressTS - ingressTS);

						//printf("ingressTS = %llu, egressTS = %llu, latency = %llu \n", ingressTS, egressTS, latency);

						#if MEASURE_LATENCIES_ON_CLIENT						
						if(initialPhase == false) {
							latencyArr[priorityID][receives[priorityID]] = latency;
							//if(receives[priorityID] < goalLoad*runtime - 1) 
							receives[priorityID]++;

							#if debug						
                            if(conn->rcnt % INTERVAL == 0 && conn->rcnt > 0) {
						        double curr_clock = now();
						        printf("T%d - %f rps, rcnt = %d, scnt = %d \n",thread_num,INTERVAL/(curr_clock-prev_clock),conn->rcnt,conn->scnt);
						        prev_clock = curr_clock;
						    }
                        	#endif
						}
						#endif

						if(!conn->pp_post_recv(conn->ctx, a, false)) ++conn->routs;
						if (conn->routs != window_size) fprintf(stderr,"Loading thread %d couldn't post receive (%d)\n", thread_num, conn->routs);
					
						//q.enqueue(17);                       // Will allocate memory if the queue is full
						//succeeded = q.try_enqueue(a-num_bufs);  // Will only succeed if the queue has an empty slot (never allocates)
						//assert(succeeded);
						//printf("succeeded \n");
						break;

					default:
						fprintf(stderr, "Completion for unknown wr_id %d\n",
							(int) wc[i].wr_id);
				}
		
				#if debug 
					printf("Thread %d rcnt = %d , scnt = %d \n",thread_num, conn->rcnt,conn->scnt);
				#endif
			}
		}
		printf("T%d recv thread outstanding send =%llu \n", thread_num, outs_send);

		if (gettimeofday(&end, NULL)) {
			perror("gettimeofday");
		}
	//} 

	float usec = (end.tv_sec - start.tv_sec) * 1000000 +(end.tv_usec - start.tv_usec);
	rps[thread_num] = conn->rcnt/(usec/1000000.);
	printf("Thread %d: %d iters in %.5f seconds, rps = %f \n", thread_num, conn->rcnt, usec/1000000., rps[thread_num]);
	//long long bytes = (long long) conn->size * conn->iters * 2;



	#if 1
	//if (thread_num < active_thread_num - 1){
        //sleep(10);
		//const int num_bufs = conn->bufs_num;
		//printf("num_bufs = %llu \n", num_bufs);
        for (uint64_t u = 0; u < 10000000; u++) {
			//if(u % 100000000 == 0) printf("u = %llu, sentCount = %llu, scnt = %llu , rcnt = %llu \n", u, conn->sentCount, conn->scnt, conn->rcnt);
            struct ibv_wc wc[num_bufs*2];
		    int ne, i;

		    ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs, wc);
            if (ne < 0) {
			    fprintf(stderr, "poll CQ failed %d\n", ne);
		    }
            for (i = 0; i < ne; ++i) {
			    if (wc[i].status != IBV_WC_SUCCESS) {
				    fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
					    ibv_wc_status_str(wc[i].status),
					    wc[i].status, (int) wc[i].wr_id);
			    }

			    int a = (int) wc[i].wr_id;
			    switch (a) {
				    case 0 ... num_bufs-1: // SEND_WRID
					    ++conn->scnt;
					    --conn->souts;
					    //#if debug
						    //printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
					    //#endif
					    break;

				    case num_bufs ... 2*num_bufs-1:
					    ++conn->rcnt;
					    --conn->routs;

						bufID = a-num_bufs;
						priorityID = (uint8_t)conn->buf_recv[bufID][50];
						
						//for(z = 42; z <= 49; z++) printf("%x ",(uint8_t)conn->buf_recv[bufID][z]);
						//printf("\n");

						egressTS = ((uint8_t)conn->buf_recv[bufID][42] << 24) + ((uint8_t)conn->buf_recv[bufID][43] << 16) + ((uint8_t)conn->buf_recv[bufID][44] << 8) + ((uint8_t)conn->buf_recv[bufID][45]);
						ingressTS = ((uint8_t)conn->buf_recv[bufID][46] << 24) + ((uint8_t)conn->buf_recv[bufID][47] << 16) + ((uint8_t)conn->buf_recv[bufID][48] << 8) + ((uint8_t)conn->buf_recv[bufID][49]);

						//egressTS = (unsigned long *)strtoul(conn->buf_recv[bufID] + 46, NULL, 0);
						
						if(egressTS > ingressTS) latency = egressTS - ingressTS;
						else latency = uint32_t(0xFFFFFFFF) + egressTS - ingressTS;

						//printf("ingressTS = %llu, egressTS = %llu, latency = %llu \n", ingressTS, egressTS, latency);

						#if MEASURE_LATENCIES_ON_CLIENT
						if(initialPhase == false) {
							latencyArr[priorityID][receives[priorityID]] = latency;
							receives[priorityID]++;
						}
						#endif

					    //#if debug
						    //printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
					    //#endif
						break;

					default:
						fprintf(stderr, "Completion for unknown wr_id %d\n",(int) wc[i].wr_id);
                }        
            }
        }
		printf("populating all_rcnt and all_scnt \n");
        all_rcnts[thread_num] = conn->rcnt;
        all_scnts[thread_num] = conn->scnt;
		printf("receiving requests done ... before barrier \n");
	    ret = pthread_barrier_wait(&barrier);
		printf("receiving requests done ... after barrier \n");
		//terminate_load = true;
		terminate_all = true;

    //}

    if(conn->rcnt != conn->scnt) printf("\n T%d DID NOT DRAIN! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, conn->rcnt,conn->scnt,conn->routs,conn->souts);
    else printf("\n T%d DRAINED! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, conn->rcnt,conn->scnt,conn->routs,conn->souts);
    #endif

	//insert barrier to send last pkt after receive threads have drained.
	//ret2 = pthread_barrier_wait(&barrier2);

	ret = pthread_join(pt, NULL);
	assert(!ret);

	//sleep(15);
	printf("after sleep \n");
	//sending final packet to quit server threads
	#if 1
	if(thread_num == active_thread_num - 1) {
		//int num_bufs = conn->sync_bufs_num;
		for (int i = 0; i < 1; i++) {
			//send one pkt with 0xFFFF
			conn->buf_send[i][1] = 0; //used to temrinate server
			conn->buf_send[i][0] = 0; //used to temrinate server
			conn->buf_send[i][2] = 0; //seq number
			conn->buf_send[i][3] = 255; //used to temrinate server
			conn->buf_send[i][10] = 0; //priority

			//printf("sending after barrier, dest_qpn = %llu \n", conn->dest_qpn);

			int success = conn->pp_post_send(conn->ctx, remote_qp0 /*conn->dest_qpn*/, conn->size, i);
			
			//printf("sending last pkt \n");
			if (success == EINVAL) printf("Invalid value provided in wr \n");
			else if (success == ENOMEM)	printf("Send Queue is full or not enough resources to complete this operation \n");
			else if (success == EFAULT) printf("Invalid value provided in qp \n");
			else if (success != 0) {
				printf("success = %d, \n",success);
				fprintf(stderr, "Couldn't post send 2 \n");
			}
			else {
				++conn->souts;
				#if debug
					printf("send posted... souts = %d, \n",conn->souts);
				#endif
			}

			#if RR
				conn->offset = ((SERVER_THREADS-1) & (conn->offset+1));
				conn->dest_qpn = remote_qp0+conn->offset;
			#endif

			#if RANDQP
				conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
			#endif
		}
		//printf("exited while loop \n");
	}
	#endif
	

	//if(thread_num == active_thread_num - 1){
	//	rps[thread_num] = conn->sync_iters/(usec/1000000.);
	//	printf("Meas. Thread: %d iters in %.5f seconds, rps = %f \n", conn->rcnt, usec/1000000., conn->rcnt/(usec/1000000.));
	//}
	//else if (thread_num < active_thread_num - 1) {

	//}

	// Dump out measurement results from measurement thread
	if(thread_num == active_thread_num - 1) {
		//printf("hello thread %llu \n",thread_num);
		//terminate_load = true;
		//printf("Measurement done, terminating loading threads\n");
        all_rcnts[thread_num] = conn->rcnt;
        all_scnts[thread_num] = conn->scnt;
	    //ret = pthread_barrier_wait(&barrier);
		//printf("hello2\n");

		//sleep(5);
        uint32_t total_rcnt = 0;
        uint32_t total_scnt = 0;
		//aggregate RPS
		for(int i = 0; i < active_thread_num; i++){
    		printf("rps = %d \n",(int)rps[i]);
			totalRPS += rps[i];
		}
		for(int i = 0; i < active_thread_num; i++){
            total_rcnt += all_rcnts[i];
            total_scnt += all_scnts[i];
		}
		//printf("avgRPS = %f \n",totalRPS/active_thread_num-1);
		printf("total RPS = %d, total rcnt = %d, total scnt = %d \n", (int)totalRPS,(int)total_rcnt,(int)total_scnt) ;
		//sleep(10);

		/*
		char* output_name;
    	asprintf(&output_name, "%s/%d_%d_%d.result", output_dir, window_size, active_thread_num, (int)totalRPS);
		FILE *f = fopen(output_name, "wb");
		for (int i=0; i<conn->sync_iters; i++) {
			float latency = (float)conn->measured_latency[i];
			fprintf(f, "%.5f \n", latency);
		}
		fclose(f);
		

    	asprintf(&output_name, "%s/%d_%d_%d_%.0f.time", output_dir, window_size, active_thread_num, (int)totalRPS, usec/1000000);
		FILE *ftime = fopen(output_name, "wb");
		float run_time = (float)(usec/1000000);
		fprintf(ftime, "%.5f \n", run_time);
		fclose(ftime);
		*/
		

        //wait to receive pkt
		
		/*
		struct timespec requestStart, requestEnd;
        bool received = false;
		while (received == false) {
				struct ibv_wc wc[num_bufs*2];
				int ne, i;

				do {
					ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs, wc);
					if (ne < 0) {
						fprintf(stderr, "poll CQ failed %d\n", ne);
					}
					
					#if debug
						printf("thread_num %d polling \n",thread_num);
					#endif

				} while (!conn->use_event && ne < 1);

				for (i = 0; i < ne; ++i) {
					if (wc[i].status != IBV_WC_SUCCESS) {
						fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
							ibv_wc_status_str(wc[i].status),
							wc[i].status, (int) wc[i].wr_id);
					}

					int a = (int) wc[i].wr_id;
					switch (a) {
						case 0 ... num_bufs-1: // SEND_WRID
							++conn->scnt;
							--conn->souts;

							clock_gettime(CLOCK_MONOTONIC, &requestStart);

							#if debug
								printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif
							break;

						case num_bufs ... 2*num_bufs-1:

							clock_gettime(CLOCK_MONOTONIC, &requestEnd);

                            //printf("latency = %f ns \n",(requestEnd.tv_sec-requestStart.tv_sec)/1e-9 +(requestEnd.tv_nsec-requestStart.tv_nsec));
							++conn->rcnt;
							--conn->routs;
							received = true;
							#if debug
								printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif

                            {
                            uint32_t egress_in0 = (uint)conn->buf_recv[a-num_bufs][42];
                            uint32_t egress_in1 = (uint)conn->buf_recv[a-num_bufs][43];
                            uint32_t egress_in2 = (uint)conn->buf_recv[a-num_bufs][44];
                            uint32_t egress_in3 = (uint)conn->buf_recv[a-num_bufs][45];
                            uint32_t egress_in_tot = (egress_in3&0xff) | ( (egress_in2&0xff) << 8) | ( (egress_in1&0xff) << 16) | ( (egress_in0&0xff) << 24);

                            printf("egress in - 0: %x, 1: %x, 2: %x, 3: %x, total: %x , %lu \n",egress_in0&0xff,egress_in1&0xff,egress_in2&0xff,egress_in3&0xff, egress_in_tot,egress_in_tot);

                            uint8_t egress_out0 = (uint)conn->buf_recv[a-num_bufs][46];
                            uint8_t egress_out1 = (uint)conn->buf_recv[a-num_bufs][47];
                            uint8_t egress_out2 = (uint)conn->buf_recv[a-num_bufs][48];
                            uint8_t egress_out3 = (uint)conn->buf_recv[a-num_bufs][49];
                            uint32_t egress_out_tot = egress_out3 | egress_out2 << 8 | egress_out1 << 16 | egress_out0 << 24;

                            printf("egress out - 0: %x, 1: %x, 2: %x, 3: %x, total: %x , %lu \n",egress_out0&0xff,egress_out1&0xff,egress_out2&0xff,egress_out3&0xff, egress_out_tot,egress_out_tot);

                            uint8_t ingress_in0 = (uint)conn->buf_recv[a-num_bufs][50];
                            uint8_t ingress_in1 = (uint)conn->buf_recv[a-num_bufs][51];
                            uint8_t ingress_in2 = (uint)conn->buf_recv[a-num_bufs][52];
                            uint8_t ingress_in3 = (uint)conn->buf_recv[a-num_bufs][53];
                            uint32_t ingress_in_tot = ingress_in3 | ingress_in2 << 8 | ingress_in1 << 16 | ingress_in0 << 24;

                            printf("ingress in - 0: %x, 1: %x, 2: %x, 3: %x, total: %x , %lu \n",ingress_in0&0xff,ingress_in1&0xff,ingress_in2&0xff,ingress_in3&0xff, ingress_in_tot,ingress_in_tot);

                            uint8_t ingress_out0 = (uint)conn->buf_recv[a-num_bufs][54];
                            uint8_t ingress_out1 = (uint)conn->buf_recv[a-num_bufs][55];
                            uint8_t ingress_out2 = (uint)conn->buf_recv[a-num_bufs][56];
                            uint8_t ingress_out3 = (uint)conn->buf_recv[a-num_bufs][57];
                            uint32_t ingress_out_tot = ingress_out3 | ingress_out2 << 8 | ingress_out1 << 16 | ingress_out0 << 24;

                            printf("ingress out - 0: %x, 1: %x, 2: %x, 3: %x, total: %x , %lu \n",ingress_out0&0xff,ingress_out1&0xff,ingress_out2&0xff,ingress_out3&0xff, ingress_out_tot,ingress_out_tot);
                            }
							
							break;

						default:
							fprintf(stderr, "Completion for unknown wr_id %d\n",
								(int) wc[i].wr_id);
							//return 1;
					}
				
					#if debug 
						printf("Thread %d rcnt = %d , scnt = %d \n",thread_num, conn->rcnt,conn->scnt);
					#endif
				}
		}
		
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        printf("Captured pkt count \n");
		*/
        printf("totalRPS = %d\n", (int)totalRPS);
	}
	
	//printf("hello thread %llu \n",thread_num);

	if (conn->pp_close_ctx(conn->ctx))
		printf("Thread %d couldn't close ctx \n",thread_num);

	ibv_free_device_list(conn->dev_list);
	
	//free(rem_dest);
	free(conn->measured_latency);

	return 0;
}
	


int main(int argc, char *argv[])
{
	int c;
	while ((c = getopt (argc, argv, "w:t:d:g:v:q:m:s:r:p:c:l:n:z:")) != -1)
    switch (c)
	{
      case 'w':
        window_size = atoi(optarg);
        break;
      case 't':
        active_thread_num = atoi(optarg); //adding one for the measurement thread
        break;
      case 'd':
        output_dir = optarg;
        break;
	  case 'g':
		gidx_in = atoi(optarg);
		break;
	  case 'v':
	  	ib_devname_in = optarg;
		break;
	  case 'q':
	  	remote_qp0_in = atoi(optarg);
		break;
	  case 'm':
	  	distribution_mode = atoi(optarg);
		break;
	  case 's':
	  	servername = optarg;
		break;
      case 'r':
        bimodal_ratio = atoi(optarg);
        break;
      case 'p':
        long_query_percent = atoi(optarg);
        break;
	  case 'c':
		SERVER_THREADS = atoi(optarg);
		break;
	  case 'l':
		goalLoad = atoi(optarg);
		break;
	  case 'n':
		numberOfPriorities = atoi(optarg);
		break;
	  case 'z':
		tcp_port = atoi(optarg);
		break;
      default:
	  	printf("Unrecognized command line argument\n");
        return 0;
    }

	printf("window size is %d; thread number is %d; output dir is %s; devname is %s, gidx is %d, mode is %d, server ip = %s, ratio = %d, percent = %d \n", window_size, active_thread_num, output_dir, ib_devname_in, gidx_in, distribution_mode, servername, bimodal_ratio,  long_query_percent);

	assert(active_thread_num >= 1);

	#if MEASURE_LATENCIES_ON_CLIENT
	//latencies.resize(active_thread_num, vector<vector<uint64_t>>(numberOfPriorities, vector<uint64_t>(0,0)));
	
	latencies = (uint64_t ***)malloc(active_thread_num*sizeof(uint64_t**));
	for(uint64_t b = 0; b < active_thread_num; b++) {
		latencies[b] = (uint64_t**)malloc(numberOfPriorities*sizeof(uint64_t*));
		for(uint64_t p = 0; p < numberOfPriorities; p++) {
			latencies[b][p] = (uint64_t*)malloc(goalLoad*runtime*sizeof(uint64_t));
			for(uint64_t v = 0; v < goalLoad*runtime; v++) {
				latencies[b][p][v] = 0;
			}
		}
	}	
	#endif

	//q = ReaderWriterQueue<uint64_t>(window_size);

    rps = (double*)malloc((active_thread_num)*sizeof(double));
    all_rcnts = (uint32_t*)malloc(active_thread_num*sizeof(uint32_t));
    all_scnts = (uint32_t*)malloc(active_thread_num*sizeof(uint32_t));

    sendDistribution = (uint64_t**)malloc(active_thread_num*sizeof(uint64_t*));
	for(int i = 0; i < active_thread_num; i++) {
		sendDistribution[i] = (uint64_t*)malloc(numberOfPriorities*sizeof(uint64_t));
		for(int j = 0; j < numberOfPriorities; j++) sendDistribution[i][j] = 0;
	}

    recvDistribution = (uint64_t**)malloc(active_thread_num*sizeof(uint64_t*));
	for(int i = 0; i < active_thread_num; i++) {
		recvDistribution[i] = (uint64_t*)malloc(numberOfPriorities*sizeof(uint64_t));
		for(int j = 0; j < numberOfPriorities; j++) recvDistribution[i][j] = 0;
	}
	vector<RDMAConnection *> connections;
	for (int i = 0; i < active_thread_num; i++) {
		RDMAConnection *conn;
		connections.push_back(conn);
	}

	ret = pthread_barrier_init(&barrier, &attr, active_thread_num);
	assert(ret == 0);

	ret2 = pthread_barrier_init(&barrier2, &attr2, 2*active_thread_num);


	uint8_t ib_port = 1;
	do_uc(ib_devname_in, servername, tcp_port, ib_port, gidx_in, 1);
	sleep(3);

  	struct thread_data tdata [connections.size()];
	pthread_t pt[active_thread_num];
	for(int i = 0; i < active_thread_num; i++){
		tdata[i].conn = connections[i];  
		tdata[i].id = i;
		tdata[i].latencyArr = latencies[i];
		tdata[i].sends = sendDistribution[i];
		tdata[i].receives = recvDistribution[i];
		int ret = pthread_create(&pt[i], NULL, client_threadfunc, &tdata[i]);
		assert(ret == 0);
		if(i == 0) sleep(3);
	}

	my_sleep(10*1000000000);
	printf("warmup complete! \n");
	warmUpFinished = true;

	my_sleep(runtime*1000000000);
	printf("after sleep \n");
	terminate_load = true;

	for(int i = 0; i < active_thread_num; i++){
		int ret = pthread_join(pt[i], NULL);
		assert(!ret);
	}

	#if MEASURE_LATENCIES_ON_CLIENT
	char* output_name_all;
	asprintf(&output_name_all, "%s/all_%d.result", output_dir, (int)totalRPS);
	FILE *f_all = fopen(output_name_all, "wb");


	printf("dumping latencies ... \n");
	for(uint i = 0; i < numberOfPriorities; i++){

		char* output_name;
		asprintf(&output_name, "%s/%d_%d.result", output_dir, i, (int)totalRPS);
		FILE *f = fopen(output_name, "wb");

		for(uint j = 0; j < active_thread_num; j++) {
			uint64_t index = 0;
			while(latencies[j][i][index] > 0) {
				fprintf(f, "%llu \n", latencies[j][i][index]);
				fprintf(f_all, "%llu \n", latencies[j][i][index]);
				index++;
			}
		}
		fclose(f);
	}
	fclose(f_all);
	#endif

	printf("start distribution ... \n");
	printf("send ..... recv \n");

	bool tooManyDrops = false;
	for(int i = 0; i < numberOfPriorities; i++){
		uint64_t sendCount = 0;
		uint64_t recvCount = 0;
		for(int t = 0; t < active_thread_num; t++) recvCount += recvDistribution[t][i];
		for(int t = 0; t < active_thread_num; t++) sendCount += sendDistribution[t][i];
		printf("%llu ... %llu \n",sendCount,recvCount);		
		if(sendCount-recvCount > 0.15*sendCount) tooManyDrops = true;
	}		
	printf("end distribution ... \n");
	printf("%d\n", (uint)totalRPS);
	printf("%d\n", tooManyDrops);

	char* output_name;
	asprintf(&output_name, "%s/%d.drop", output_dir, (int)totalRPS);
	FILE *f = fopen(output_name, "wb");
	fprintf(f, "%llu \n", tooManyDrops);
	fclose(f);
}
