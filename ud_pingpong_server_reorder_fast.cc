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
#include <linux/types.h>  //for __be32 type
#include "ServerRDMAConnection.cc"
#include <vector>
#include <assert.h>
#include <pthread.h> 
#include <assert.h>


#include <set>
#include <list>
#include "rdma_uc.cc"
#include <immintrin.h>
#include <x86intrin.h>
#include <cstdlib>
//#include <intrin.h>

#define debug 0
#define debugFPGA 0
#define DRAIN_SERVER 0
#define MEAS_TIME_ON_SERVER 0
#define ENABLE_HT 0
#define ENABLE_SERV_TIME 1
#define RR 0
#define COUNT_IDLE_POLLS 0
#define MEAS_POLL_LAT 0
#define MEAS_POLL_LAT_INT 100000
#define MEAS_POLL_LAT_WARMUP 10001

#define MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ 0

#define FPGA_NOTIFICATION 1
//if SHARED_CQ is 1, this should be zero (for strict policy)

#define PROCESS_IN_ORDER 0
#define STRICT_PRIORITY 0
#define ROUND_ROBIN 0
#define WEIGHTED_ROUND_ROBIN 0
#define ASSERT 1  //should be same as REORDER pragma in client
#define PRINT 0


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
vector<list<recvReq> > metaCQ; //(numberOfPriorities , list<uint16_t>(0,0));
set<uint16_t> skipList;


uint32_t * all_rcnts;
uint32_t * all_scnts;
vector<RDMAConnection *> connections;


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
uint64_t tcp_port = 20001;

//vector<vector<uint16_t> > indexOfNonZeroPriorities (numberOfPriorities , vector<uint16_t> (0, 0));

uint64_t count64 = 0;
uint64_t count01 = 0;
uint64_t count11 = 0;
uint64_t countOther = 0;

void     INThandler(int);

#if MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ
	unsigned long start_cycle = 0;
	unsigned long end_cycle = 0; 
	double execution_time_cycles = 0;
	double elapsedExecutionTimeMeasurements = 0;
#endif

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
	// printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
        printf("  -l, --sl=<SL>          send messages with service level <SL> (default 0)\n");
	printf("  -e, --events           sleep on CQ events (default poll)\n");
	printf("  -g, --gid-idx=<gid index> local port gid index\n");
	printf("  -c, --chk              validate received buffer\n");
}
*/

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);

	printf("count64= %llu, countOther = %llu, count01= %llu, count11= %llu , rcnt = %llu \n", count64, countOther, count01, count11, connections[0]->rcnt);
	#if MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ
		printf("avg = %f, \n ", (float(execution_time_cycles)/elapsedExecutionTimeMeasurements));
	#endif
	/*
	#if COUNT_IDLE_POLLS
		printf("printing idle polls of all threads \n");
		double avgIdlePolls = 0.0;
		for(unsigned int i = 0; i < NUM_THREADS; i++) {
			printf("idle polls  = %llu \n", idlePolls[i]);
			printf("total polls = %llu \n", totalPolls[i]);
			printf("%f, \n ", float(idlePolls[i])/float(totalPolls[i]));
			printf("\n");
			avgIdlePolls += float(idlePolls[i])/float(totalPolls[i]);
		}
		printf("\n \n");
		printf("avg = %f, \n ", float(avgIdlePolls)/NUM_THREADS);
	#endif
	*/
	exit(0);
}

void* server_threadfunc(void* x) {

	struct thread_data *tdata = (struct thread_data*) x;
	//RDMAConnection *conn = tdata->conn;
	vector<RDMAConnection *> connections = *(tdata->conn);
	int thread_num = tdata->id;

	int offset = thread_num%NUM_QUEUES;
	RDMAConnection *conn = connections[offset];

	//conn = new RDMAConnection(tdata->id, ib_devname_in, gidx_in ,servername);


	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset

    #if ENABLE_HT
        if(thread_num < 8) CPU_SET(thread_num, &cpuset);  //set CPU 2 on cpuset
	    else CPU_SET(24+(thread_num%8), &cpuset);
    #else
        CPU_SET(thread_num, &cpuset);  //set CPU 2 on cpuset
    #endif

	printf("T%d - qp = 0x%06x , %d, offset = %d \n",thread_num,conn->my_dest.qpn,conn->my_dest.qpn, offset);

	if(thread_num == 0) do_uc(ib_devname_in, server_name, tcp_port, conn->ib_port, gidx_in, NUM_QUEUES);

	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	const int num_bufs = recv_bufs_num;

	if (gettimeofday(&conn->start, NULL)) {
		perror("gettimeofday");
	}
	#if MEAS_TIME_ON_SERVER
		struct timespec requestStart, requestEnd;
        float sum = 0;
	#endif

	unsigned int rcnt, scnt = 0;
	bool receivedFirstOne = false;

	#if MEAS_POLL_LAT
		struct timespec beginPoll,endPoll;
		//double sumPollTime = 0.0;
		bool printedPoll = true;
		double pollTime = 0.0;
	#endif

	uint16_t signalInterval = conn->rx_depth-recv_bufs_num;
	int16_t priority = -1;
	uint64_t bufID;
	uint64_t srcQP;
	#if PROCESS_IN_ORDER
		//list <recvReq> inOrderArrivals;
		recvReq *inOrderArrivals = (recvReq *)malloc(recv_bufs_num*sizeof(recvReq));
		for(uint16_t index = 0; index < recv_bufs_num; index++) {
			inOrderArrivals[index].bufID = 0;
			inOrderArrivals[index].srcQP = 0;
		}
		uint16_t head, tail, size = 0;
	#endif

	uint8_t checkCnt = 0;
	uint64_t notifCount = 0;

	#if WEIGHTED_ROUND_ROBIN
		uint16_t *queueWeights = (uint16_t*)malloc(numberOfPriorities*sizeof(uint16_t));
		uint16_t tempWeight = 10;
		for(int z = 0; z < numberOfPriorities; z++) {
			//queueWeights[z] = (numberOfPriorities-z);
			queueWeights[z] = tempWeight;
			if(z%(numberOfPriorities/10) == (numberOfPriorities/10)-1) tempWeight--;
		}
		//printf("Weights \n");
		//for(int z = 0; z < numberOfPriorities; z++) printf("%llu \n",queueWeights[z]);
	#endif

	set<uint16_t>::iterator skipListIter = skipList.begin();
	set<uint16_t>::iterator tempskipListIter = skipList.begin();
	set<uint16_t>::iterator skipListIterPrev = skipList.begin();
	//int16_t skipListPrioPrev = -1;

	uint16_t qpID = 0;
	bool received = false;

	bool isZero = true;

	signal(SIGINT, INThandler);	

	//__m512i zero = 0;
	//zero = _mm512_setzero_si512();

	#if FPGA_NOTIFICATION
		//volatile __m512i** inputVector = reinterpret_cast<volatile __m512i**>(res.input512);
		//volatile __mmask8** maskVector = reinterpret_cast<volatile __mmask8**>(res.mask512);
		//volatile __m512i** outputVector = reinterpret_cast<volatile __m512i**>(res.output512);
	#endif

	#if FPGA_NOTIFICATION
		uint32_t *processBufB4Polling = (uint32_t*) malloc(NUM_QUEUES*sizeof(uint32_t)); 
		for(int q = 0; q < NUM_QUEUES; q++) {
			//processBufB4Polling[q] = (uint32_t*) malloc(sizeof(uint32_t));
			processBufB4Polling[q] = NULL;
		}
		uint32_t *processBufB4PollingSrcQP = (uint32_t*) malloc(NUM_QUEUES*sizeof(uint32_t)); 
		for(int q = 0; q < NUM_QUEUES; q++) {
			//processBufB4PollingSrcQP[q] = (uint32_t*) malloc(sizeof(uint32_t));
			processBufB4PollingSrcQP[q] = NULL;
		}
		uint32_t connIndex = 0;		
	#endif

	while (1) {
		#if RR
			conn = connections[offset];
			//printf("offset = %d \n",offset);
			//offset = ((NUM_QUEUES-1) & (offset+1));	
			if(offset == NUM_QUEUES-1) offset = 0;
			else offset++;	
		#endif
	
		struct timespec ttime,curtime;

		
		//conn = connections[0];

		#if FPGA_NOTIFICATION
			//printf("Contents of FPGA notification= %llu \n", htonll(*(res.buf)));
			//conn = connections[NUM_QUEUES - 1 - __builtin_clzll(htonll(*(res.buf)))];

			//only for debugging purposes
			/*
			unsigned long long tempBuf = htonll(*(res.buf));
			while(1) 
			{
				if(htonll(*(res.buf)) == tempBuf) continue;
				else {
					tempBuf = htonll(*(res.buf));
					printf("Contents of FPGA notification= %llu, connection index = %d \n", htonll(*(res.buf)), NUM_QUEUES - 1 - __builtin_clzll(htonll(*(res.buf))));
					break;
				}
			}
			*/
			
			if(*(res.buf) == 0x00) {
				#if debugFPGA
				printf("byte vector is 0x00 \n");
				#endif
				continue;
			}
			else {
				//assert(*(res.buf) == 0xFF);
				#if debugFPGA
				printf("byte vector is %x, setting to 0x00 \n", *(res.buf));
				#endif

				*(res.buf) == 0x00;
				connIndex = 0; //this value will change depending on leading zeros
				conn = connections[connIndex];

				if(processBufB4Polling[connIndex] != NULL && processBufB4PollingSrcQP[connIndex] != NULL) {
					#if debugFPGA
					printf("found work item from previous iter, process that \n");
					#endif

					bufID = (processBufB4Polling[connIndex]);
					srcQP = (processBufB4PollingSrcQP[connIndex]);
					received = true;
					goto process;
				}
			}
			

			/*
			//my_sleep(1,1);
			unsigned long long tmp = __builtin_clzll(htonll(*(res.buf)) & 0x00000000000000FF);
			//printf("FPGA notification= %x, connection index = %llu \n", tmp, tmp);
			//printf("hi \n");
			if(tmp == 56) {
				count64++;
				if(isZero) count01++;
				else count11++;
				isZero = false;
				conn = connections[0];
			}
			else {
				countOther++;
				//printf("FPGA notification= %x, connection index = %llu \n", tmp, tmp);
				continue;
			}

			if(count64 == 1) countOther = 0;
			*/

			//printf("offset = %d \n",offset);
			//offset = ((NUM_QUEUES-1) & (offset+1));	
			//if(offset == NUM_QUEUES-1) offset = 0;
			//else offset++;	

			#if 0
			//use this for clz!!
			int p = 0;
			unsigned long start_cycle = readTSC(); 
			unsigned long long leadingZeros = 0;

			//for(p = 0; p < 4; p++) *outputVector[p] = _mm512_mask_lzcnt_epi64(*outputVector[p], *maskVector[p], *inputVector[p]);
			//for(p = 0; p < 4; p++) *outputVector[p] = _mm512_lzcnt_epi64(*(inputVector[p]));
			
			//volatile __m512i s;
			//volatile __m512i rest;
  			//rest = _mm512_mask_lzcnt_epi64 (rest, 2, s);

			// EXECUTE KERNEL 
			for(uint64_t x = 0; x < 1000000; x++) {
				//int random = rand() % 32; //can write a random number so branch predictor mispredicts
				//*(res.buffers[random]) = 64;
				bool found = false;
				for(p = 0; p < 16; p++) {
					
					__asm__ __volatile__ ("lzcnt %1, %0" : "=r" (leadingZeros) : "r" (htonll(*(res.buffers[p]))):);
					if(leadingZeros < 64) {
						found = true;
						//printf("inside loop p = %d, buffers[%d] = %llu , clz = %llu \n", p, p, htonll(*(res.buffers[p])), _lzcnt_u64(htonll(*(res.buffers[p]))));
						break;
					}
					
					/*
					if(_lzcnt_u64(htonll(*(res.buffers[p]))) < 64) {
						//printf("inside loop p = %d, buffers[%d] = %llu , clz = %llu \n", p, p, htonll(*(res.buffers[p])), _lzcnt_u64(htonll(*(res.buffers[p]))));
						found = true;
						break;
					}
					*/
					//if(_lzcnt_u64(htonll(*(res.buffers[p]))) < 64) {
					//__m512i zero;
					//_mm512_lzcnt_epi64(zero);
				}
				if(found == false) {
					for(p = 16; p < 32; p++) {
						
						__asm__ __volatile__ ("lzcnt %1, %0" : "=r" (leadingZeros) : "r" (htonll(*(res.buffers[p]))):);
						if(leadingZeros < 64) {
							//printf("inside loop p = %d, buffers[%d] = %llu , clz = %llu \n", p, p, htonll(*(res.buffers[p])), _lzcnt_u64(htonll(*(res.buffers[p]))));
							break;
						}
						
						/*
						if(_lzcnt_u64(htonll(*(res.buffers[p]))) < 64) {
							//printf("inside loop p = %d, buffers[%d] = %llu , clz = %llu \n", p, p, htonll(*(res.buffers[p])), _lzcnt_u64(htonll(*(res.buffers[p]))));
							break;
						}
						*/
						//if(_lzcnt_u64(htonll(*(res.buffers[p]))) < 64) {
						//__m512i zero;
						//_mm512_lzcnt_epi64(zero);
					}
				}
				//*(res.buffers[random]) = 0;
			}

			//printf("outside loop p = %d \n", p);
			unsigned long end_cycle = readTSC(); 
			//double execution_time_seconds = ((double) (end_cycle-start_cycle)) / TSC_FREQUENCY;
			printf("clz elapsed = %f , p = %llu \n", (double) (end_cycle-start_cycle)/1000000, p);
			#endif

		#endif

		#if SHARED_CQ
			struct ibv_wc wc[num_bufs*2];
		#else
			struct ibv_wc wc[1];
		#endif
		
		int ne, i, j, prio;

		#if DRAIN_SERVER
			uint64_t empty_cnt = 0;
			do {
				ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs , wc);				
				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
				}
				else if (ne == 0) empty_cnt++;

				if(empty_cnt >= 1000000000) break;
				#if debug
					//printf("T%d polling \n",thread_num);
				#endif

			} while (!conn->use_event && ne < 1);

			if(empty_cnt >= 1000000000) break;
		#else
			//do {
				//ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs , wc);
				#if MEAS_POLL_LAT
					clock_gettime(CLOCK_MONOTONIC,&beginPoll);
				#endif

				//unsigned long start_cycle = readTSC(); 
				//for(uint64_t x = 0; x < 1000000; x++) {

				#if SHARED_CQ
					ne = ibv_poll_cq(ctxGlobal->cq, num_bufs*2 , wc);
				#else 
					#if debugFPGA
					printf("poll (1) \n");
					#endif
					ne = ibv_poll_cq(conn->ctx->cq, 1 , wc);
				#endif
				//}
				//unsigned long end_cycle = readTSC(); 
				//printf("clz elapsed = %f \n", (double) (end_cycle-start_cycle)/1000000);

				//printf("polled ne = %d \n",ne);

				#if MEAS_POLL_LAT
					clock_gettime(CLOCK_MONOTONIC,&endPoll);	
					if(rcnt >= MEAS_POLL_LAT_WARMUP) {
						pollTime = (endPoll.tv_sec-beginPoll.tv_sec )/1e-9 + (endPoll.tv_nsec-beginPoll.tv_nsec);
						sumPollTime[thread_num] += pollTime;
						totalPolls[thread_num]++;
						/*
						if(rcnt % MEAS_POLL_LAT_INT == 0 && printedPoll == false) {
							printf("T%d : sumPollTime = %f, avgPollTime = %f ns \n",thread_num,sumPollTime[thread_num], sumPollTime[thread_num]/MEAS_POLL_LAT_INT);
							sumPollTime = 0;
							printedPoll = true;
						}
						*/
					}
				#endif	

				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
				}

				//#if !SHARED_CQ && FPGA_NOTIFICATION
					if (ne == 0) {
						#if debugFPGA
						printf("no work item in poll (1) \n");
						#endif
						continue;
					}
				//#endif

				//if(totalPolls[thread_num] ==  0xFFFFFFFFFFFFFFFF) totalPollMSBs[thread_num]++;
				//else 
				#if COUNT_IDLE_POLLS
					if(receivedFirstOne == false) {
						if(ne > 0) {
							receivedFirstOne = true;	
							totalPolls[thread_num] = 0;
							idlePolls[thread_num] = 0;
						}
					}
					else {
						totalPolls[thread_num]++;

						if(ne == 0) {
							if(idlePolls[thread_num] == 0xFFFFFFFFFFFFFFFF) printf("idle wrapped\n"); //idlePolls[thread_num]++;
							//else 
							idlePolls[thread_num]++;
						}
					}
					//printf("idle  = %llu \n", idlePolls[thread_num]);

				#endif

			//} while (!conn->use_event && ne < 1);
		#endif

		#if debugFPGA
		printf("found work item in poll (1) \n");
		#endif
        for (i = 0; i < ne; ++i) {
            if (wc[i].status != IBV_WC_SUCCESS) {
                fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
                    ibv_wc_status_str(wc[i].status),
                    wc[i].status, (int) wc[i].wr_id);
            }
        
            uint16_t a = (int) wc[i].wr_id;

            switch (a) {
            case 0 ... num_bufs-1:

				qpID = a/bufsPerQP;
				conn = connections[qpID];

				conn->scnt++;
                ++scnt;
                --conn->souts;

                #if debug
                    printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
                #endif

                break;
            case num_bufs ... (2*num_bufs)-1:
                #if MEAS_TIME_ON_SERVER
                    clock_gettime(CLOCK_MONOTONIC, &requestStart);
                #endif

                #if MEAS_POLL_LAT
                    printedPoll = false;
                #endif

				#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY || PROCESS_IN_ORDER
				 	recvReq x; 
					x.bufID = a;
					x.srcQP = wc[i].src_qp;
				#endif

				#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY
					uint8_t priorityID = (uint)buf_recv[a-num_bufs][50];
					skipList.insert(priorityID);
					metaCQ[priorityID].emplace_back(x);
				#endif

				#if PROCESS_IN_ORDER
					//inOrderArrivals.emplace_back(x);

					if(head == tail && size == recv_bufs_num) continue;
					else if(head == recv_bufs_num-1) head = 0;
					else head++;
					inOrderArrivals[head] = x;
					size++;
				#endif

				#if !SHARED_CQ
					bufID = a;
					srcQP = wc[i].src_qp;
					received = true;

					#if MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ
						end_cycle = readTSC(); 
						if(start_cycle != 0) {
							execution_time_cycles = ((double) (end_cycle-start_cycle));
							elapsedExecutionTimeMeasurements++;
						}
					#endif
				#endif

                break;
            }
        }
		//poll n process 1, poll n
	//#if PROCESS_IN_ORDER
	//	while(!skipList.empty() && skipList.size() != 0) {
	//#else

process:

#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY
	if(!skipList.empty() && skipList.size() != 0) {
#elif PROCESS_IN_ORDER
	if(size > 0) { //inOrderArrivals.size() > 0) {
#elif !SHARED_CQ 
	if(received == true) {
		received = false;
#endif

		#if STRICT_PRIORITY && !PROCESS_IN_ORDER
            set<uint16_t>::iterator iter = skipList.begin();
			priority = *iter;

			assert(metaCQ[priority].size() > 0);
			recvReq tmp = metaCQ[priority].front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			metaCQ[priority].pop_front();
			if(metaCQ[priority].size() == 0) skipList.erase(iter); //skipList.erase(priority);
			//assert(priority == (uint)conn->buf_recv[bufID-num_bufs][50]);
		#endif

		#if ROUND_ROBIN && !PROCESS_IN_ORDER

			//the problem is we don't want to update pointer and then poll. we want to poll and then update.
			//poll update process
			//the priority update in cycle n should consider what priority was processed in cycle n-1

			//poll
			//update pointer
			//erase previous pointer (next cycle)
			//save previous pointer
			//process one item

			if(skipList.size() == 1 || priority == -1) skipListIter = skipList.begin();
			else {
				if (*skipListIter == *skipList.rbegin()) skipListIter = skipList.begin();
				else advance(skipListIter, 1); 
			}

			if (priority != -1) {
				if(metaCQ[priority].size() == 0) skipList.erase(skipListIterPrev);
			}

			if(skipList.size() == 0) {
				priority = -1; //resetting priority to we don't get stuck in loop
				continue;
			}
			priority = *skipListIter;
			skipListIterPrev = skipListIter;

			assert(metaCQ[priority].size() > 0);
			recvReq tmp = metaCQ[priority].front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			metaCQ[priority].pop_front();


			
			//assert(priority == (uint)conn->buf_recv[bufID-num_bufs][50]);
    
		#endif
		#if WEIGHTED_ROUND_ROBIN && !PROCESS_IN_ORDER

			if(skipList.size() == 1 || priority == -1) skipListIter = skipList.begin();
			else if (checkCnt == queueWeights[priority] || metaCQ[priority].size() == 0) {
				if (*skipListIter == *skipList.rbegin()) skipListIter = skipList.begin();
				else advance(skipListIter, 1); 
				checkCnt = 0;
			}

			if (priority != -1) {
				if(metaCQ[priority].size() == 0) skipList.erase(skipListIterPrev);
			}

			if(skipList.size() == 0) {
				priority = -1; //resetting priority to we don't get stuck in loop
				continue;
			}

			priority = *skipListIter;
			skipListIterPrev = skipListIter;

			assert(metaCQ[priority].size() > 0);
			recvReq tmp = metaCQ[priority].front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			metaCQ[priority].pop_front();

			checkCnt++;

		#endif

		#if PROCESS_IN_ORDER && !STRICT_PRIORITY && !ROUND_ROBIN && !WEIGHTED_ROUND_ROBIN
			//assert(size > 0);//inOrderArrivals.size() > 0);

			if(head == tail && size == 0) continue;
			else if(tail == recv_bufs_num-1) tail = 0;
			else tail++;

			recvReq tmp = inOrderArrivals[tail];//.front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			size--;
			//printf("arrivals size = %llu \n", size);
			//inOrderArrivals.pop_front();
		#endif


		#if PROCESS_IN_ORDER && WEIGHTED_ROUND_ROBIN

			if(skipList.size() == 1 || priority == -1) skipListIter = skipList.begin();
			else if (checkCnt == queueWeights[priority] || metaCQ[priority].size() == 0) {
				if (*skipListIter == *skipList.rbegin()) skipListIter = skipList.begin();
				else advance(skipListIter, 1); 
				checkCnt = 0;
			}

			if (priority != -1) {
				if(metaCQ[priority].size() == 0) skipList.erase(skipListIterPrev);
			}

			if(skipList.size() == 0) {
				priority = -1; //resetting priority to we don't get stuck in loop
				continue;
			}

			priority = *skipListIter;
			skipListIterPrev = skipListIter;

			assert(metaCQ[priority].size() > 0);
			recvReq temp = metaCQ[priority].front();
			bufID = temp.bufID;
			srcQP = temp.srcQP;
			metaCQ[priority].pop_front();

			checkCnt++;

			if(head == tail && size == 0) continue;
			else if(tail == recv_bufs_num-1) tail = 0;
			else tail++;

			recvReq tmp = inOrderArrivals[tail];//.front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			size--;
		#endif


		#if PROCESS_IN_ORDER && ROUND_ROBIN

			if(skipList.size() == 1 || priority == -1) skipListIter = skipList.begin();
			else {
				if (*skipListIter == *skipList.rbegin()) skipListIter = skipList.begin();
				else advance(skipListIter, 1); 
			}

			if (priority != -1) {
				if(metaCQ[priority].size() == 0) skipList.erase(skipListIterPrev);
			}

			if(skipList.size() == 0) {
				priority = -1; //resetting priority to we don't get stuck in loop
				continue;
			}
			priority = *skipListIter;
			skipListIterPrev = skipListIter;

			assert(metaCQ[priority].size() > 0);
			recvReq temp = metaCQ[priority].front();
			bufID = temp.bufID;
			srcQP = temp.srcQP;
			metaCQ[priority].pop_front();

			if(head == tail && size == 0) continue;
			else if(tail == recv_bufs_num-1) tail = 0;
			else tail++;

			recvReq tmp = inOrderArrivals[tail];//.front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			size--;
		#endif


		#if PROCESS_IN_ORDER && STRICT_PRIORITY
            set<uint16_t>::iterator iter = skipList.begin();
			priority = *iter;

			assert(metaCQ[priority].size() > 0);
			recvReq temp = metaCQ[priority].front();
			bufID = temp.bufID;
			srcQP = temp.srcQP;
			metaCQ[priority].pop_front();
			if(metaCQ[priority].size() == 0) skipList.erase(iter); //skipList.erase(priority);
			//assert(priority == (uint)conn->buf_recv[bufID-num_bufs][50]);

			if(head == tail && size == 0) continue;
			else if(tail == recv_bufs_num-1) tail = 0;
			else tail++;

			recvReq tmp = inOrderArrivals[tail];//.front();
			bufID = tmp.bufID;
			srcQP = tmp.srcQP;
			size--;
		#endif

		#if PRINT
			printf("priority = %d , checkCnt = %d \n", priority, checkCnt);
			printf("skipList, size = %d \n", skipList.size());
			for(set<uint16_t>::iterator it = skipList.begin(); it != skipList.end(); it++)
			{
				printf(" %d (%d) ",*it,metaCQ[*it].size());
			}
			printf("\n");
		

			#if PROCESS_IN_ORDER
				printf("inOrderArrivals \n");
				for(uint16_t u = 0; u < inOrderArrivals.size(); u++)
				{
					printf(" %d (%d) ",inOrderArrivals[u]-num_bufs, (uint)buf_recv[inOrderArrivals[u]-num_bufs][50]);
					//printf(" %d, ",(uint)buf_recv[inOrderArrivals[u]-num_bufs][50]);
				}
				printf("\n");
			#endif
					
			//printf("priority is %d, and buf prio is %d \n",priority,(uint)conn->buf_recv[bufID-num_bufs][50]);
		#endif

			//assert makes sure QPs using correct QP
			#if ASSERT
				/*
				if(((bufID-num_bufs)/bufsPerQP) != ((uint8_t)buf_recv[bufID-num_bufs][50])) {
					printf("calculated prio = %d, buffer prio = %d \n",((bufID-num_bufs)/bufsPerQP),((uint)buf_recv[bufID-num_bufs][50]));
					printf("inOrderArrivals \n");
					for(uint16_t u = 0; u < inOrderArrivals.size(); u++)
					{
						printf(" %llu (%llu) ",inOrderArrivals[u]-num_bufs, (uint8_t)buf_recv[inOrderArrivals[u]-num_bufs][50]);
						//printf(" %d, ",(uint)buf_recv[inOrderArrivals[u]-num_bufs][50]);
					}
					printf("\n");
				}
				*/
				assert(((bufID-num_bufs)/bufsPerQP) == ((uint8_t)buf_recv[bufID-num_bufs][50]));
			#endif

			#if SHARED_CQ
				qpID = (bufID-num_bufs)/bufsPerQP;
				conn = connections[qpID];
			#endif

			conn->rcnt++;
			++rcnt;
            --conn->routs;

            #if debug
                if(rcnt % 10000000 == 0) printf("T%d - rcnt = %d, scnt = %d \n",thread_num,rcnt,scnt);
            #endif

            #if debug
                printf("T%d - recv complete, bufID = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,bufID,rcnt,scnt,conn->routs,conn->souts);
            #endif
            
            uint8_t sleep_int_lower = (uint)buf_recv[bufID-num_bufs][41];
            uint8_t sleep_int_upper = (uint)buf_recv[bufID-num_bufs][40];	

            uint8_t checkByte2 = (uint)buf_recv[bufID-num_bufs][42];	
            uint8_t checkByte3 = (uint)buf_recv[bufID-num_bufs][43];	

            #if ENABLE_SERV_TIME
                //if (sleep_int_upper<0) sleep_int_upper+= 256;
                //if (sleep_int_lower<0) sleep_int_lower+= 256;	
                unsigned int sleep_time = (sleep_int_lower + sleep_int_upper * 0x100) << 4;
                //unsigned int sleep_time = (sleep_int_lower + ((sleep_int_upper << 8) & 0x100)) << 4;
                //if(sleep_time > 10000) printf("sleep_int_lower = %lu, sleep_int_upper = %lu, sleep_time = %lu \n",sleep_int_lower, sleep_int_upper, sleep_time);
                my_sleep(sleep_time, thread_num);
                
                /*
                conn->buf_send[a-num_bufs][1] = sleep_int_lower;
                conn->buf_send[a-num_bufs][0] = sleep_int_upper;
                */
                //if((uint)conn->buf_recv[a-num_bufs][42] == 255 && (uint)conn->buf_recv[a-num_bufs][43] == 255) 
				
				//counting number of notifications from the FPGA
				//if((uint)buf_recv[bufID-num_bufs][53] == 153) notifCount++;
                
				for(int q = 0; q <= 18; q++) {
					//printf(" %x ", (uint)conn->buf_recv[bufID-num_bufs][q+40]);
					buf_send[bufID-num_bufs][q] = (uint)buf_recv[bufID-num_bufs][q+40];
				}
				//printf("\n\n");
				//if(sleep_int_lower == 0 && sleep_int_upper == 0 && checkByte2 == 255 && checkByte3 == 255) printf("notificationCount = %llu \n", notifCount);
            #endif

            #if COUNT_IDLE_POLLS
                if(sleep_int_lower == 0 && sleep_int_upper == 0 && checkByte2 == 255 && checkByte3 == 255) {
                    printf("printing idle polls of all threads \n");
                    double avgIdlePolls = 0.0;
                    for(unsigned int i = 0; i < NUM_THREADS; i++) {
                        printf("idle polls  = %llu \n", idlePolls[i]);
                        printf("total polls = %llu \n", totalPolls[i]);
                        printf("%f, \n ", float(idlePolls[i])/float(totalPolls[i]));
                        printf("\n");
                        avgIdlePolls += float(idlePolls[i])/float(totalPolls[i]);
                    }
                    printf("\n \n");
                    printf("avg = %f, \n ", float(avgIdlePolls)/NUM_THREADS);
                    printf("rcnt = %d \n",rcnt);
                    printf("scnt = %d \n",scnt);
                    rcnt = 0;
                    scnt = 0;
                }
            #endif

            #if MEAS_POLL_LAT
                if(sleep_int_lower == 0 && sleep_int_upper == 0 && checkByte2 == 255 && checkByte3 == 255) {
                    printf("avg polls time of all threads \n");
                    double sumAllPollTimes = 0.0;
                    for(unsigned int i = 0; i < NUM_THREADS; i++) {
                        sumAllPollTimes += float(sumPollTime[i])/float(totalPolls[i]);
                    }
                    printf("\n \n");
                    printf("avg = %f, \n ", float(sumAllPollTimes)/NUM_THREADS);
                }
            #endif

            conn->routs += !(conn->pp_post_recv(conn->ctx, bufID));
            //if (conn->routs != num_bufs) fprintf(stderr,"Couldn't post receive (%d)\n",conn->routs);

            int success = conn->pp_post_send(conn->ctx, srcQP /*conn->rem_dest->qpn*/, conn->size , bufID-num_bufs, (conn->rcnt%signalInterval == 0));
            //printf("src qp = %d \n",srcQP);

            //printf("src qp = %d \n",wc[i].src_qp);
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
            #if MEAS_TIME_ON_SERVER
                clock_gettime(CLOCK_MONOTONIC, &requestEnd);
                //if(rcnt > 200000000) printf("latency = %f ns \n",(requestEnd.tv_sec-requestStart.tv_sec)/1e-9 +(requestEnd.tv_nsec-requestStart.tv_nsec));
                sum = sum + ((requestEnd.tv_sec-requestStart.tv_sec)/1e-9 +(requestEnd.tv_nsec-requestStart.tv_nsec));
                if(rcnt % 10000000 == 0) {
                    printf("sum = %f, avg latency = %f ns \n",sum, sum/10000000);
                    sum = 0;
                }
                //}
            #endif
					
			#if MEAS_TIME_BETWEEN_PROCESSING_TWO_REQ
				start_cycle = readTSC(); 
			#endif
        }
		#if ROUND_ROBIN
			//priority++;
			//if(priority == numberOfPriorities) priority = 0;
		#endif

		#if WEIGHTED_ROUND_ROBIN
			//checkCnt++;
			//if(checkCnt == queueWeights[priority]) {
			//	checkCnt = 0;
			//	priority++;
			//	if(priority == numberOfPriorities) priority = 0;
			//}
		#endif

		#if FPGA_NOTIFICATION
			#if debugFPGA
			printf("poll (2) \n");
			#endif
			bool foundRecv = false;
			for(int iter = 0; iter < 2; iter++) {
				if(foundRecv == true) break;
				ne = ibv_poll_cq(conn->ctx->cq, 1 , wc);

				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
				}

				if (ne > 0) {
					#if debugFPGA
					printf("found work item in poll (2) , setting vector to 0xFF \n");
					#endif
					//isZero = true;
					//continue;

					for (i = 0; i < ne; ++i) {
						if (wc[i].status != IBV_WC_SUCCESS) {
							fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
								ibv_wc_status_str(wc[i].status),
								wc[i].status, (int) wc[i].wr_id);
						}
					
						uint16_t a = (int) wc[i].wr_id;

						switch (a) {
						case 0 ... num_bufs-1:

							qpID = a/bufsPerQP;
							conn = connections[qpID];

							conn->scnt++;
							++scnt;
							--conn->souts;

							#if debugFPGA
							printf("found send completion in completion queue \n");
							#endif

							#if debug
								printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
							#endif

							break;
						case num_bufs ... (2*num_bufs)-1:
							#if MEAS_TIME_ON_SERVER
								clock_gettime(CLOCK_MONOTONIC, &requestStart);
							#endif

							#if !SHARED_CQ
								#if debugFPGA
								printf("b4 setting work item metadata for next iter \n");
								#endif
								*(res.buf) = 0xFF; //htonll(0x0000000000000000);

								(processBufB4Polling[connIndex]) = a;
								(processBufB4PollingSrcQP[connIndex]) = wc[i].src_qp;
								#if debugFPGA
								printf("after setting work item metadata for next iter \n");
								#endif

								foundRecv = true;
							#endif

							break;
						}
					}
				}
				else {
					#if debugFPGA
					printf("no work item in poll (2) \n");
					#endif
					processBufB4Polling[connIndex] = NULL;
					processBufB4PollingSrcQP[connIndex] = NULL;
					break;
				}
			}
		#endif





///////////////////////////
		//printf("size of vector indexOfNonZeroPriorities is: %d \n", indexOfNonZeroPriorities.size());
#if ROUND_ROBIN || WEIGHTED_ROUND_ROBIN || STRICT_PRIORITY || PROCESS_IN_ORDER || !SHARED_CQ
	}
#endif
//end of while(1) loop

	if (gettimeofday(&conn->end, NULL)) {
		perror("gettimeofday");
	}


	/*
	#if 1
	//if (thread_num < active_thread_num - 1){
	sleep(10);
	for (int u=0; u<1000000; u++) {
	    //if(thread_num == active_thread_num - 1) int num_bufs = conn->sync_bufs_num;
	    //else int num_bufs = conn->bufs_num;

	    const int num_bufs = recv_bufs_num;

	    struct ibv_wc wc[recv_bufs_num*2];
		    int ne, i;
			#if SHARED_CQ
		    	ne = ibv_poll_cq(ctxGlobal->cq, 2*num_bufs, wc);
			#else
		    	ne = ibv_poll_cq(conn->ctx->cq, 2*num_bufs, wc);
			#endif

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
					++scnt;
					--conn->souts;
					//#if debug
						//printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
					//#endif
					break;

				case num_bufs ... 2*num_bufs-1:
					++rcnt;
					--conn->routs;
					//#if debug
						//printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
					//#endif
			}        
	    }
	}

	if(rcnt != scnt) printf("\n T%d DID NOT DRAIN! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, rcnt,scnt,conn->routs,conn->souts);
	else printf("\n T%d DRAINED! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, rcnt,scnt,conn->routs,conn->souts);
	#endif
	*/





	if (scnt != rcnt) fprintf(stderr, "Different send counts and receive counts for thread %d\n", thread_num);

        all_rcnts[thread_num] = rcnt;
        all_scnts[thread_num] = scnt;

	if (conn->pp_close_ctx(conn->ctx, NUM_QUEUES)) {
		printf("close ctx returned 1\n");
	}

	ibv_free_device_list(conn->dev_list);
	//free(conn->rem_dest);

	return 0;
}

int main(int argc, char *argv[])
{
	
	int c;
	while ((c = getopt (argc, argv, "w:t:d:g:v:q:m:s:r:p:n:z:")) != -1)
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
      default:
	  	printf("Unrecognized command line argument\n");
        return 0;
    }

	//RDMAConnection *conn = new RDMAConnection(0, ib_devname_in, gidx_in ,servername);

	//printf("size of vector indexOfNonZeroPriorities is: %d \n", indexOfNonZeroPriorities.size());


////
//#include <iostream>
//#include <iterator>
//#include <set>

	/*
    printf("creating set \n");

    set<uint16_t> s;
    set<uint16_t>::iterator iter = s.begin();
    printf(" \n ", *iter);

    s.insert(5);     printf("inserted 5 \n");
    s.insert(3);     printf("inserted 3 \n");


    printf(" %d \n ", *iter);


    advance(iter,1); printf("advancec iter by 1\n");
    
    printf(" %d \n ", *iter);

    
    s.insert(1); printf("inserted 1 \n");

    //s.insert(6);
    //s.insert(3);
    //s.insert(7);
    //s.insert(2);
    
    printf(" %d \n ", *iter);
    //s.erase(1); printf("erased 1 \n");
    s.erase(3);   printf("erased 3 \n");
    //s.erase(5); printf("erased 5 \n");
    
    //before erasing, if set.size() == 1. iter = NULL
                      //else if (*iter == *s.rbegin()) iter = s.begin();
					  //else advance(iter, 2); 
    
    //when inserting, if set.size() == 1. iter = set.begin()
    
    iter = s.begin(); printf("setting to rbegin \n");
    
    printf(" %d \n ", *s.rbegin() );
    advance(iter, 2); printf("advanced iter by 2 \n");
    printf(" %d \n ", *iter);


    
	printf("printing the whole set \n");
    for(iter = s.begin(); iter!=s.end();advance(iter,1))     printf(" %d \n ", *iter);
	*/
////

	metaCQ.resize(numberOfPriorities , list<recvReq>(0,{0,0}));
	bufsPerQP = recv_bufs_num/NUM_QUEUES;

	for (uint i = 0; i < NUM_QUEUES; i++) {
		//if(i > 500) { 
		//printf("before creating connection i = %lu \n",i);
		//RDMAConnection *conn;
		connections.push_back(new RDMAConnection(i, ib_devname_in, gidx_in ,servername, NUM_QUEUES));
		//printf("after creating connection i = %lu \n",i);
	}

	all_rcnts = (uint32_t*)malloc(NUM_THREADS*sizeof(uint32_t));
	all_scnts = (uint32_t*)malloc(NUM_THREADS*sizeof(uint32_t));

	#if COUNT_IDLE_POLLS
		idlePolls = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
		//idlePollMSBs = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
		totalPolls = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
		//totalPollMSBs = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
	#endif

	#if MEAS_POLL_LAT
		sumPollTime = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
		totalPolls = (uint64_t*)calloc(NUM_THREADS,sizeof(uint64_t));
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
            total_rcnt += all_rcnts[i];
            total_scnt += all_scnts[i];
	}
	printf("total rcnt = %d, total scnt = %d \n",(int)total_rcnt,(int)total_scnt);
}
