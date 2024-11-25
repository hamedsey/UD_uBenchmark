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

#include <infiniband/verbs.h>
#include <linux/types.h>  //for __be32 type
#include "ServerRDMAConnection.cc"
#include <vector>
#include <assert.h>
#include <pthread.h> 

#define debug 0
#define DRAIN_SERVER 0
#define MEAS_TIME_ON_SERVER 0
#define ENABLE_HT 0
#define ENABLE_SERV_TIME 1
#define RR 0
#define COUNT_IDLE_POLLS 0
#define MEAS_POLL_LAT 0
#define MEAS_POLL_LAT_INT 100000
#define MEAS_POLL_LAT_WARMUP 10001

uint32_t * all_rcnts;
uint32_t * all_scnts;

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


char* servername;
char *ib_devname_in;
int gidx_in;
int NUM_THREADS = 4;
int NUM_QUEUES = 55;



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

	printf("T%d - qp = 0x%06x , %d \n",thread_num,conn->my_dest.qpn,conn->my_dest.qpn);

	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	const int num_bufs = conn->recv_bufs_num;

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

	uint16_t signalInterval = conn->rx_depth-conn->recv_bufs_num;

	while (1) {
		#if RR
			offset = ((NUM_QUEUES-1) & (offset+1));		
			conn = connections[offset];
		#endif

		//struct ibv_wc wc[1];
		struct ibv_wc wc[num_bufs*2];
		
		int ne, i;

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

				ne = ibv_poll_cq(conn->ctx->cq, num_bufs*2 , wc);

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

				//if(totalPolls[thread_num] ==  0xFFFFFFFFFFFFFFFF) totalPollMSBs[thread_num]++;
				//else 
				#if COUNT_IDLE_POLLS
					if(receivedFirstOne == false) {
						if(ne > 0) {
							receivedFirstOne = true;	
						}
					}
					else {
						totalPolls[thread_num]++;

						if(ne == 0) {
							//if(idlePolls[thread_num] == 0xFFFFFFFFFFFFFFFF) idlePollMSBs[thread_num]++;
							//else 
							idlePolls[thread_num]++;
						}
					}

				#endif

			//} while (!conn->use_event && ne < 1);
		#endif


		for (i = 0; i < ne; ++i) {
			if (wc[i].status != IBV_WC_SUCCESS) {
				fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
					ibv_wc_status_str(wc[i].status),
					wc[i].status, (int) wc[i].wr_id);
			}
		
			int a = (int) wc[i].wr_id;

			switch (a) {
			case 0 ... num_bufs-1:

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

				conn->rcnt++;
				++rcnt;
				--conn->routs;

				#if debug
					if(rcnt % 10000000 == 0) printf("T%d - rcnt = %d, scnt = %d \n",thread_num,rcnt,scnt);
				#endif

				#if debug
					printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,rcnt,scnt,conn->routs,conn->souts);
				#endif
				
				uint8_t sleep_int_lower = (uint)conn->buf_recv[a-num_bufs][41];
				uint8_t sleep_int_upper = (uint)conn->buf_recv[a-num_bufs][40];	

				uint8_t checkByte2 = (uint)conn->buf_recv[a-num_bufs][42];	
				uint8_t checkByte3 = (uint)conn->buf_recv[a-num_bufs][43];	

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
					for(int q = 0; q <= 18; q++) conn->buf_send[a-num_bufs][q] = (uint)conn->buf_recv[a-num_bufs][q+40];
					/*
					if(sleep_int_lower == 0 && sleep_int_upper == 0 && checkByte2 == 255 && checkByte3 == 255) {
						printf("rcnt = %d \n",rcnt);
						printf("scnt = %d \n",scnt);
					}
					*/
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

				conn->routs += !(conn->pp_post_recv(conn->ctx, a));
				//if (conn->routs != num_bufs) fprintf(stderr,"Couldn't post receive (%d)\n",conn->routs);

				//int success = conn->pp_post_send(conn->ctx, wc[i].src_qp /*conn->rem_dest->qpn*/, conn->size , a-num_bufs);
				int success = conn->pp_post_send(conn->ctx, wc[i].src_qp /*conn->rem_dest->qpn*/, conn->size , a-num_bufs, (conn->rcnt%signalInterval == 0));

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
				
				break;
			}
		}
	}

	if (gettimeofday(&conn->end, NULL)) {
		perror("gettimeofday");
	}



	#if 1
	//if (thread_num < active_thread_num - 1){
	sleep(10);
	for (int u=0; u<1000000; u++) {
	    //if(thread_num == active_thread_num - 1) int num_bufs = conn->sync_bufs_num;
	    //else int num_bufs = conn->bufs_num;

	    const int num_bufs = conn->recv_bufs_num;

	    struct ibv_wc wc[conn->recv_bufs_num*2];
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





	if (scnt != rcnt) fprintf(stderr, "Different send counts and receive counts for thread %d\n", thread_num);

        all_rcnts[thread_num] = rcnt;
        all_scnts[thread_num] = scnt;

	if (conn->pp_close_ctx(conn->ctx)) {
		printf("close ctx returned 1\n");
	}

	ibv_free_device_list(conn->dev_list);
	//free(conn->rem_dest);

	return 0;
}

int main(int argc, char *argv[])
{
	
	int c;
	while ((c = getopt (argc, argv, "w:t:d:g:v:q:m:s:r:p:")) != -1)
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
      default:
	  	printf("Unrecognized command line argument\n");
        return 0;
    }

	//RDMAConnection *conn = new RDMAConnection(0, ib_devname_in, gidx_in ,servername);

	vector<RDMAConnection *> connections;
	for (uint i = 0; i < NUM_QUEUES; i++) {
		if(i > 500) { printf("i = %lu \n",i); sleep(1);}
		//RDMAConnection *conn;
		connections.push_back(new RDMAConnection(i, ib_devname_in, gidx_in ,servername));
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
		if(i == 0) sleep(1);
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
