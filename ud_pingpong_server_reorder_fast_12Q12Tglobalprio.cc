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
#include  <signal.h>
#include <infiniband/verbs.h>

#define DO_UC 1

#if DO_UC
	#include "rdma_uc.cc"
#else 
	#include "rdma_rc.c"
#endif

#include <linux/types.h>  //for __be32 type
#include "ServerRDMAConnection.cc"
#include <vector>
#include <assert.h>
#include <pthread.h> 
#include <assert.h>


#include <set>
#include <list>

#include <immintrin.h>
#include <x86intrin.h>
#include <cstdlib>
//#include <intrin.h>

#include <iostream>
#include <mutex>
//using namespace std;

#define debug 0
#define debugFPGA 0
#define DRAIN_SERVER 0
#define MEAS_TIME_ON_SERVER 0
#define ENABLE_HT 0
#define ENABLE_SERV_TIME 1

#define RR_POLL 0
#define WRR_POLL 0
#define STRICT_POLL 0

#define COUNT_IDLE_POLLS 0
#define MEAS_POLL_LAT 0
#define MEAS_POLL_LAT_INT 100000
#define MEAS_POLL_LAT_WARMUP 10001

#define MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ 0

#define FPGA_NOTIFICATION 0
//if SHARED_CQ is 1, this should be zero (for strict policy)

#define PROCESS_IN_ORDER 0
#define STRICT_PRIORITY 1
#define ROUND_ROBIN 0
#define WEIGHTED_ROUND_ROBIN 0
#define ASSERT 0  //should be same as REORDER pragma in client //disable if using SRQ (shared receive queue)
#define PRINT 0

#define IDEAL 0 //enable shared CQ with this

#define SCALEOUT 0

#define GLOBAL_STRICT_PRIORITY 1
#define NUM_GROUPS 1

#define NO_PRIORITY 0




			//1000
			//0100
			//1100
			//0010
			//1010
			//0001
			//1001

struct recvReq {
	uint64_t bufID;
	uint64_t srcQP;
};

uint64_t numberOfPriorities = 0;



uint32_t * all_rcnts;
uint32_t * all_scnts;
vector<RDMAConnection *> connections;

pthread_barrier_t barrier; 
pthread_barrierattr_t attr;
int ret; 


#if COUNT_IDLE_POLLS
	uint64_t * idlePolls;
	//uint64_t * idlePollMSBs;
	uint64_t * totalPolls;
	//uint64_t * totalPollMSBs;
#endif

#if MEAS_POLL_LAT
	uint64_t * sumPollTime;
	uint64_t * totalPolls;
#endif


char* servername = NULL; //used for UD
char* server_name = NULL; //used for UC

char *ib_devname_in;
int gidx_in;
int NUM_THREADS = 4;
int NUM_QUEUES = 55;
uint64_t tcp_port = 20002;
uint64_t buffersPerQ = 100;
bool terminateRun = false;

//vector<vector<uint16_t> > indexOfNonZeroPriorities (numberOfPriorities , vector<uint16_t> (0, 0));

uint64_t count64 = 0;
uint64_t count01 = 0;
uint64_t count11 = 0;
uint64_t countOther = 0;
//uint64_t idlePolls = 0;

uint64_t **countPriority;

#if GLOBAL_STRICT_PRIORITY
	vector<list<recvReq> > metaCQ[NUM_GROUPS];
	set<uint16_t> skipList[NUM_GROUPS];
	set<uint16_t>::iterator skipListIter[NUM_GROUPS];
	set<uint16_t>::iterator tempskipListIter[NUM_GROUPS];
	set<uint16_t>::iterator skipListIterPrev[NUM_GROUPS];
	mutex skipListMutex[NUM_GROUPS];
	//mutex metaCQMutex;
#endif

void     INThandler(int);

////////rdtsc

#define TSC_FREQUENCY ((double)2.2E9) 
inline unsigned long readTSC() { 
	_mm_mfence(); // wait for earlier memory instructions to retire 
	//_mm_lfence(); // block rdtsc from executing 
	unsigned long long tsc = __rdtsc(); 
	_mm_mfence();
	//_mm_lfence(); // block later instructions from executing 
	return tsc; 
}


/////////////



inline void my_sleep(uint n, int thread_num) {
	//if(thread_num ==0) printf("mysleep = %d \n",n);
	struct timespec ttime,curtime;
	clock_gettime(CLOCK_MONOTONIC,&ttime);
	
	while(1){
		clock_gettime(CLOCK_MONOTONIC,&curtime);
		double elapsed = (curtime.tv_sec-ttime.tv_sec )/1e-9 + (curtime.tv_nsec-ttime.tv_nsec);

		if (elapsed >= n) {
			break;
		}
	}

	return;
}

struct thread_data{
	//RDMAConnection *conn;
	vector<RDMAConnection *> *conn;
	int id;
};


void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);

	printf("count64= %llu, countOther = %llu, count01= %llu, count11= %llu , rcnt = %llu \n", count64, countOther, count01, count11, connections[0]->rcnt);
	//printf("idle polls = %llu \n", idlePolls);
	#if MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ
		printf("avg = %f, \n ", (float(execution_time_cycles)/elapsedExecutionTimeMeasurements));
	#endif

	exit(0);
}

void* server_threadfunc(void* x) {

	struct thread_data *tdata = (struct thread_data*) x;
	//RDMAConnection *conn = tdata->conn;
	vector<RDMAConnection *> connections = *(tdata->conn);
	int thread_num = tdata->id;
	uint64_t threadGroupSize = NUM_THREADS/NUM_GROUPS;
	uint64_t queueGroupSize = NUM_QUEUES/NUM_GROUPS;
	uint64_t groupID = thread_num/threadGroupSize;
	uint64_t groupOffset = groupID*queueGroupSize;

	int offset = thread_num%NUM_QUEUES;
	printf("T%d, offset = %d, groupID = %d, groupOffset = %d \n",thread_num, offset, groupID, groupOffset);

	RDMAConnection *conn = connections[offset];

	//conn = new RDMAConnection(tdata->id, ib_devname_in, gidx_in ,servername);

	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset

    #if ENABLE_HT
        if(thread_num < 8) CPU_SET(thread_num, &cpuset);  //set CPU 2 on cpuset
	    else CPU_SET(24+(thread_num%8), &cpuset);
    #else
        if(thread_num < 12) CPU_SET(thread_num, &cpuset);  //set CPU 2 on cpuset
		else CPU_SET(thread_num + 12, &cpuset);
    #endif

	#if DO_UC 
		if(thread_num == 0) do_uc(ib_devname_in, server_name, tcp_port, conn->ib_port, gidx_in, NUM_QUEUES, NUM_THREADS);
	#else
		if(thread_num == 0) do_rc(ib_devname_in, server_name, tcp_port, conn->ib_port, gidx_in, NUM_QUEUES);
	#endif

	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	int num_bufs = recv_bufs_num;

	if (gettimeofday(&conn->start, NULL)) {
		perror("gettimeofday");
	}
	#if MEAS_TIME_ON_SERVER
		struct timespec requestStart, requestEnd;
        float sum = 0;
	#endif

	unsigned int rcnt, scnt = 0;
	bool receivedFirstOne = false;

	uint16_t signalInterval = (conn->rx_depth-recv_bufs_num)/16;
	if(recv_bufs_num > conn->rx_depth) signalInterval = (recv_bufs_num-conn->rx_depth)/2;
	printf("signalInterval = %llu, rx_depth = %llu, recvbufsnum = %llu, num_bufs = %llu \n",signalInterval, conn->rx_depth, recv_bufs_num, num_bufs);
	int16_t priority = -1;
	uint64_t bufID;
	uint64_t srcQP;

	uint8_t checkCnt = 0;
	uint64_t notifCount = 0;

	skipListIter[groupID] = skipList[groupID].begin();
	tempskipListIter[groupID] = skipList[groupID].begin();
	skipListIterPrev[groupID] = skipList[groupID].begin();

	//uint16_t qpID = 0;
	bool received = false;

	bool isZero = true;
	uint64_t i = 0;
	uint64_t j = 0;
	uint64_t a = 0;
	bool firstPoll = true;
	signal(SIGINT, INThandler);	

	ret = pthread_barrier_wait(&barrier);


	while (terminateRun == false) {
		struct ibv_wc wc[num_bufs*2];
		
		int ne, prio;
		i = 0; j = 0;
		ne = ibv_poll_cq(conn->ctx->cq, num_bufs*2 , wc);

		if (ne < 0) {
			fprintf(stderr, "poll CQ failed %d\n", ne);
		}
		else if (ne == 0) continue;
		//else if (ne > 0) printf("thread %llu , offset = %llu, polled %llu items \n", thread_num, offset, ne);

        for (i = 0; i < ne; ++i) {
			//i = 0;
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "2 Failed status %s (%d) for wr_id %d\n",
                    ibv_wc_status_str(wc[i].status),
                    wc[i].status, (int) wc[i].wr_id);
            }
        
            a = (int) wc[i].wr_id;

			if(a >= 0 && a < num_bufs) {

				conn->scnt++;
                ++scnt;
                --conn->souts;

                #if debug
                    printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
                #endif
			}
			else {
				#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY || PROCESS_IN_ORDER
				 	recvReq x; 
					x.bufID = a;
					x.srcQP = wc[i].src_qp;
				#endif

				#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY
					uint8_t priorityID = (uint)buf_recv[a-num_bufs][52];
					
					#if GLOBAL_STRICT_PRIORITY
					skipListMutex[groupID].lock();
					skipList[groupID].insert(priorityID);
					#else 
					skipList.insert(priorityID);
					#endif
					
					#if GLOBAL_STRICT_PRIORITY
					metaCQ[groupID][priorityID].emplace_back(x);
					//printf("T%llu received priority = %llu \n",thread_num, priorityID);
					skipListMutex[groupID].unlock();
					#else
					metaCQ[priorityID].emplace_back(x);
					#endif
					
				#endif
			}
        }
		
process:
#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY
	#if GLOBAL_STRICT_PRIORITY
	skipListMutex[groupID].lock();
	if(!skipList[groupID].empty() && skipList[groupID].size() != 0) {
	#endif
#endif
	

		#if STRICT_PRIORITY && !PROCESS_IN_ORDER
			#if GLOBAL_STRICT_PRIORITY 
			set<uint16_t>::iterator iter = skipList[groupID].begin();
			#else 
			set<uint16_t>::iterator iter = skipList.begin();
			#endif

			priority = *iter;
			#if GLOBAL_STRICT_PRIORITY 
			assert(metaCQ[groupID][priority].size() > 0);
			recvReq tmp = metaCQ[groupID][priority].front();
			#else
			assert(metaCQ[priority].size() > 0);
			recvReq tmp = metaCQ[priority].front();
			#endif

			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			#if GLOBAL_STRICT_PRIORITY 
			metaCQ[groupID][priority].pop_front();
			if(metaCQ[groupID][priority].size() == 0) skipList[groupID].erase(iter); //skipList.erase(priority);
			#else
			metaCQ[priority].pop_front();
			if(metaCQ[priority].size() == 0) skipList.erase(iter); //skipList.erase(priority);
			#endif

			#if GLOBAL_STRICT_PRIORITY
			skipListMutex[groupID].unlock();
			#endif

			//assert(priority == (uint)buf_recv[bufID-num_bufs][52]);
		#endif

		conn->rcnt++;
		++rcnt;
		--conn->routs;

		#if debug
			if(rcnt % 10000000 == 0) printf("T%d - rcnt = %d, scnt = %d \n",thread_num,rcnt,scnt);
		#endif

		countPriority[thread_num][(uint8_t)buf_recv[bufID-num_bufs][52]]++;
		uint8_t sleep_int_lower = (uint)buf_recv[bufID-num_bufs][41];
		uint8_t sleep_int_upper = (uint)buf_recv[bufID-num_bufs][40];	

		uint8_t checkByte2 = (uint)buf_recv[bufID-num_bufs][42];	
		uint8_t checkByte3 = (uint)buf_recv[bufID-num_bufs][43];	
		if(checkByte2 == 255 && checkByte3 == 255) {
			printf("terminating run \n");
			terminateRun = true;
			break;
		}

		#if ENABLE_SERV_TIME

			unsigned int sleep_time = (sleep_int_lower + sleep_int_upper * 0x100) << 4;

			my_sleep(sleep_time, thread_num);
			

			memcpy(buf_send[bufID-num_bufs],buf_recv[bufID-num_bufs]+40,20);
		#endif

		conn->routs += !(conn->pp_post_recv(conn->ctx, bufID));

		int success = conn->pp_post_send(conn->ctx, srcQP /*conn->rem_dest->qpn*/, conn->size , bufID-num_bufs, (conn->rcnt%signalInterval == 0));

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
				//printf("send posted... souts = %d, \n",conn->souts);
			#endif
		}
	
	#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY
	}
	else {
		#if GLOBAL_STRICT_PRIORITY
		skipListMutex[groupID].unlock();
		#endif
	}
	#endif	

	}

	if (gettimeofday(&conn->end, NULL)) {
		perror("gettimeofday");
	}

        all_rcnts[thread_num] = rcnt;
        all_scnts[thread_num] = scnt;

	if(thread_num == 0) {
		if (conn->pp_close_ctx(conn->ctx, NUM_QUEUES, NUM_THREADS, thread_num)) {
			printf("close ctx returned 1\n");
		}

		ibv_free_device_list(conn->dev_list);
		//free(conn->rem_dest);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	
	int c;
	while ((c = getopt (argc, argv, "w:t:d:g:v:q:m:s:r:p:n:z:b:")) != -1)
    switch (c)
	{
      case 't':
        NUM_THREADS = atoi(optarg); //adding one for the measurement thread
        break;
	  case 'g':
		gidx_in = atoi(optarg);
		break;
	  case 'v':
	  	ib_devname_in = optarg;
		break;
	  case 's':
	  	servername = optarg;
		break;
	  case 'q':
	  	NUM_QUEUES = atoi(optarg);
		break;
	  case 'n':
	  	numberOfPriorities = atoi(optarg);
		break;
	  case 'z':
		tcp_port = atoi(optarg);
		break;
	  case 'b':
		buffersPerQ = atoi(optarg);
		break;
      default:
	  	printf("Unrecognized command line argument\n");
        return 0;
    }

	ret = pthread_barrier_init(&barrier, &attr, NUM_THREADS);
	assert(ret == 0);

	recv_bufs_num = buffersPerQ*NUM_QUEUES;
	bufsPerQP = buffersPerQ;//recv_bufs_num/NUM_QUEUES;
	printf("bufsPerQP = %llu, recv_bufs_num = %llu \n",bufsPerQP, recv_bufs_num);

	for (uint i = 0; i < NUM_QUEUES; i++) {

		connections.push_back(new RDMAConnection(i, ib_devname_in, gidx_in ,servername, NUM_QUEUES, NUM_THREADS));
	}

	all_rcnts = (uint32_t*)malloc(NUM_THREADS*sizeof(uint32_t));
	all_scnts = (uint32_t*)malloc(NUM_THREADS*sizeof(uint32_t));
	countPriority = (uint64_t **)malloc(NUM_THREADS*sizeof(uint64_t *));

	for(int t = 0; t < NUM_THREADS; t++) {
		countPriority[t] = (uint64_t *)malloc(numberOfPriorities*sizeof(uint64_t));
		for(int g = 0; g < numberOfPriorities; g++) countPriority[t][g] = 0;
	}

	#if GLOBAL_STRICT_PRIORITY
		for(int t = 0; t < NUM_GROUPS; t++)
			metaCQ[t].resize(numberOfPriorities , list<recvReq>(0,{0,0}));
	#endif

  	struct thread_data tdata [NUM_THREADS];
	pthread_t pt[NUM_THREADS];
	for(int i = 0; i < NUM_THREADS; i++){
		tdata[i].conn = &connections;  
		tdata[i].id = i;
		int ret = pthread_create(&pt[i], NULL, server_threadfunc, &tdata[i]);
		assert(ret == 0);
		if(i == 0) {
			sleep(1);
		}
	}

	for(int i = 0; i < NUM_THREADS; i++){
		int ret = pthread_join(pt[i], NULL);
		assert(!ret);
	}

	uint32_t total_rcnt = 0;
	uint32_t total_scnt = 0;

	for(int i = 0; i < NUM_THREADS; i++){
			//printf("allrcnt[%d] = %llu \n", i, all_rcnts[i]);
            total_rcnt += all_rcnts[i];
            total_scnt += all_scnts[i];
	}

	uint64_t *totalPerThread = (uint64_t *)malloc(NUM_THREADS*sizeof(uint64_t));
	for(int t = 0; t < NUM_THREADS; t++) totalPerThread[t] = 0;

	for(int g = 0; g < numberOfPriorities; g++) {
		printf("P%llu   ",g);

		for(int t = 0; t < NUM_THREADS; t++){
			totalPerThread[t] += countPriority[t][g];
			printf("%llu    ",countPriority[t][g]);
		}
		printf("\n");
	}
	printf("\n");
	printf("total per thread ... \n");
	for(int t = 0; t < NUM_THREADS; t++){
		printf("%llu   ",totalPerThread[t]);
	}
	printf("\n");
	printf("total rcnt = %d, total scnt = %d \n",(int)total_rcnt,(int)total_scnt);
}
