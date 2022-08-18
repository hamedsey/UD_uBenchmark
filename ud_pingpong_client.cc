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

#include <infiniband/verbs.h>
#include <linux/types.h>  //for __be32 type
#include "ClientRDMAConnection.cc"
#include <vector>
#include <sys/mman.h>
#include <pthread.h> 

//#define active_thread_num 12+1  		//n loading threads,1 meas. thread 
#define debug 0
#define INTERVAL 10000000 		//RPS MEAS INTERVAL
#define SYNC_INTERVAL 1000000 	//RPS MEAS INTERVAL

#define RR 1 				//enables round robin request distribution per thread
#define RANDQP 0				//enables random request distribution per thread
#define MEAS_RAND_NUM_GEN_LAT 0	//enables measuring latency of random number generator 
#define MEAS_GEN_LAT 0	//enables measuring latency of random number generator 

enum {
	FIXED = 0,
	NORMAL = 1,
	UNIFORM = 2,
	EXPONENTIAL = 3,
    BIMODAL = 4,
};

pthread_barrier_t barrier; 
pthread_barrierattr_t attr;
int ret; 

int remote_qp0;
int distribution_mode = EXPONENTIAL;
double *rps;
int window_size = 1;
int active_thread_num = 1;
char* output_dir;
char *ib_devname_in;
int gidx_in;
int remote_qp0_in;
char* servername;
int terminate_load = 0;
int SERVER_THREADS = 8;


//for bimodal service time distribution
int bimodal_ratio = -1;
int long_query_percent = -1;

uint32_t * all_rcnts;
uint32_t * all_scnts;


inline double now() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

struct thread_data{
	RDMAConnection *conn;
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
	printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
	printf("  -b, --debug            Enable debug prints\n");
        printf("  -l, --sl=<SL>          send messages with service level <SL> (default 0)\n");
	printf("  -e, --events           sleep on CQ events (default poll)\n");
	printf("  -g, --gid-idx=<gid index> local port gid index\n");
	printf("  -c, --chk              validate received buffer\n");
}
*/


/*
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
*/


double rand_gen() {
   // return a uniformly distributed random value
   return ( (double)(rand()) + 1. )/( (double)(RAND_MAX) + 1. );
}

int gen_latency(int mean, int mode, int isMeasThread) {
    if(isMeasThread == 1) return mean;

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
		std::random_device rd{};
		std::mt19937 gen{rd()};
		std::exponential_distribution<double> exp{1/(double)mean};
		int result = exp(gen);
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
}


uint8_t genRandDestQP(uint8_t thread_num) {          

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
	uint8_t ret = (g_lehmer32_state >> (32-2*thread_num)) % SERVER_THREADS;
	

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


void* client_threadfunc(void* x) {

	struct thread_data *tdata = (struct thread_data*) x;
	int thread_num = tdata->id;

	RDMAConnection *conn = tdata->conn;
	if(thread_num == active_thread_num - 1 && thread_num == 0) {
		conn = new RDMAConnection(thread_num,1,ib_devname_in,gidx_in,remote_qp0_in,servername);
		conn->measured_latency = (double *)malloc(sizeof(double)*(conn->sync_iters));
		remote_qp0 = rem_dest->qpn;
	}
	else if(thread_num == active_thread_num - 1) {
		conn = new RDMAConnection(thread_num,1,ib_devname_in,gidx_in,remote_qp0_in,servername);
		conn->measured_latency = (double *)malloc(sizeof(double)*(conn->sync_iters));
	}
	else if(thread_num == 0) {
		conn = new RDMAConnection(thread_num,0,ib_devname_in,gidx_in,remote_qp0_in,servername);
		remote_qp0 = rem_dest->qpn;
	}
	else conn = new RDMAConnection(thread_num,0,ib_devname_in,gidx_in,remote_qp0_in,servername);
	int offset = thread_num%SERVER_THREADS;
	printf("thread_num = %d, offset = %d, rx_depth = %d \n", thread_num, offset, conn->rx_depth);

	cpu_set_t cpuset;
    CPU_ZERO(&cpuset);       //clears the cpuset
    CPU_SET(thread_num/*+2*//*12*/, &cpuset);  //set CPU 2 on cpuset
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	sleep(1);
	ret = pthread_barrier_wait(&barrier);
	conn->dest_qpn = remote_qp0+offset;

	struct timeval start, end;
	int mean = 2000;

    printf("T%d - remote_qp0 = 0x%06x , %d,       dest_qpn = 0x%06x , %d \n",thread_num,remote_qp0,remote_qp0,conn->dest_qpn,conn->dest_qpn);

	if(thread_num == active_thread_num - 1) {
		struct timespec requestStart, requestEnd;
		const int num_bufs = conn->sync_bufs_num;
	
		for (unsigned int r = 0; r < num_bufs; ++r) {
			if(!conn->pp_post_recv(conn->ctx, r+num_bufs, true)) 
				++conn->routs;
		}
		if (conn->routs != num_bufs) fprintf(stderr,"Measurement couldn't post receive (%d)\n",conn->routs);
			
		//printf("T%d - remote_qp0 = 0x%06x , %d,       dest_qpn = 0x%06x , %d \n",thread_num,remote_qp0,remote_qp0,conn->dest_qpn,conn->dest_qpn);
		for (int i = 0; i < num_bufs; i++) {

			int req_lat = gen_latency(mean, distribution_mode,1);
			req_lat = req_lat >> 4;
            #if MEAS_GEN_LAT 
                printf("lat = %d \n",req_lat); 
            #endif
			uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
			uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;
			#if debug
				printf("lower %d; upper %d\n", lat_lower, lat_upper);
			#endif 

            //send one pkt with 0xFFFF as a test
			conn->buf_send[i][1] = 255;//lat_lower;
			conn->buf_send[i][0] = 255;//lat_upper;

			//0 signals BF pkt came from client, 1 indicates to BF pkt came from server
			conn->buf_send[i][2] = 1;

			conn->buf_send[i][3] = (conn->ctx->qp->qp_num & 0xFF0000) >> 16;
			conn->buf_send[i][4] = (conn->ctx->qp->qp_num & 0x00FF00) >> 8;
			conn->buf_send[i][5] = (conn->ctx->qp->qp_num & 0x0000FF);


			int success = conn->pp_post_send(conn->ctx, conn->dest_qpn /*conn->rem_dest->qpn*/, conn->size, i);
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
				offset = ((SERVER_THREADS-1) & (offset+1));// = (offset+1)%SERVER_THREADS;
				//if(offset == SERVER_THREADS) offset = 0;
				conn->dest_qpn = remote_qp0+offset;
			#endif

			#if RANDQP
				conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
			#endif
		}


        //wait to receive pkt
		while (conn->rcnt < 1 || conn->scnt < 1) {

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
							clock_gettime(CLOCK_MONOTONIC, &requestStart);
							++conn->scnt;
							--conn->souts;

							#if debug
								printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif
							break;

						case num_bufs ... 2*num_bufs-1:
							clock_gettime(CLOCK_MONOTONIC, &requestEnd);

                            //printf("latency = %f ns \n",(requestEnd.tv_sec-requestStart.tv_sec)/1e-9 +(requestEnd.tv_nsec-requestStart.tv_nsec));
							//conn->measured_latency[conn->rcnt] = (requestEnd.tv_sec-requestStart.tv_sec)/1e-6 +(requestEnd.tv_nsec-requestStart.tv_nsec)/1e3;

							++conn->rcnt;
							--conn->routs;
							
							#if debug
								printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif

							if(!conn->pp_post_recv(conn->ctx, a,true)) ++conn->routs;
							if (conn->routs != num_bufs) fprintf(stderr,"Measurement couldn't post receive (%d)\n",conn->routs);

							if(conn->scnt < conn->sync_iters) {
								int req_lat = gen_latency(mean, distribution_mode,1);
								req_lat = req_lat >> 4;
                                #if MEAS_GEN_LAT 
                                    printf("lat = %d \n",req_lat); 
                                #endif
								uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
								uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;

								#if debug
									printf("lower %d; upper %d\n", lat_lower, lat_upper);
								#endif

								conn->buf_send[a-num_bufs][1] = lat_lower;
								conn->buf_send[a-num_bufs][0] = lat_upper;

								int success = conn->pp_post_send(conn->ctx, conn->dest_qpn, conn->size, a-num_bufs);
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
									offset = ((SERVER_THREADS-1) & (offset+1));							
									//offset++;// = (offset+1)%SERVER_THREADS;
									//if(offset == SERVER_THREADS) offset = 0;
									//offset = (offset+1)%SERVER_THREADS;
									conn->dest_qpn = remote_qp0+offset;
								#endif		

								#if RANDQP
									conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
									//printf("dest_qpn = %d \n",conn->dest_qpn);
								#endif		

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
	    ret = pthread_barrier_wait(&barrier);
		printf("Completed test pkt! \n");
        sleep(5);
        printf("Started Measurement Thread \n");

		if (gettimeofday(&start, NULL)) {
			perror("gettimeofday");
		}
									
		double prev_clock = now();
		while (conn->rcnt < conn->sync_iters || conn->scnt < conn->sync_iters) {

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
							clock_gettime(CLOCK_MONOTONIC, &requestStart);
							++conn->scnt;
							--conn->souts;
							#if debug
								printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif
							break;

						case num_bufs ... 2*num_bufs-1:
							clock_gettime(CLOCK_MONOTONIC, &requestEnd);

                            //printf("latency = %f ns \n",(requestEnd.tv_sec-requestStart.tv_sec)/1e-9 +(requestEnd.tv_nsec-requestStart.tv_nsec));
							conn->measured_latency[conn->rcnt] = (requestEnd.tv_sec-requestStart.tv_sec)/1e-6 +(requestEnd.tv_nsec-requestStart.tv_nsec)/1e3;

							++conn->rcnt;
							--conn->routs;
							
							#if debug
								printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
							#endif

							if(conn->rcnt % SYNC_INTERVAL == 0) {
								double curr_clock = now();
								printf("Meas: T%d - %f rps, rcnt = %d, scnt = %d \n",thread_num,SYNC_INTERVAL/(curr_clock-prev_clock),conn->rcnt,conn->scnt);
								prev_clock = curr_clock;
							}

							if(!conn->pp_post_recv(conn->ctx, a,true)) ++conn->routs;
							if (conn->routs != num_bufs) fprintf(stderr,"Measurement couldn't post receive (%d)\n",conn->routs);

							if(conn->scnt < conn->sync_iters) {
								int req_lat = gen_latency(mean, distribution_mode,1);
								req_lat = req_lat >> 4;
                                #if MEAS_GEN_LAT 
                                    printf("lat = %d \n",req_lat); 
                                #endif
								uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
								uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;

								#if debug
									printf("lower %d; upper %d\n", lat_lower, lat_upper);
								#endif

								conn->buf_send[a-num_bufs][1] = lat_lower;
								conn->buf_send[a-num_bufs][0] = lat_upper;

								int success = conn->pp_post_send(conn->ctx, conn->dest_qpn, conn->size, a-num_bufs);
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
									offset = ((SERVER_THREADS-1) & (offset+1));
									//offset++;// = (offset+1)%SERVER_THREADS;
									//if(offset == SERVER_THREADS) offset = 0;
									//offset = (offset+1)%SERVER_THREADS;
									conn->dest_qpn = remote_qp0+offset;
								#endif		

								#if RANDQP
									conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
									//printf("dest_qpn = %d \n",conn->dest_qpn);
								#endif		

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

		if (gettimeofday(&end, NULL)) {
			perror("gettimeofday");
		}


	}
	else if (thread_num < active_thread_num - 1){
	    ret = pthread_barrier_wait(&barrier);
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

		for (int i = 0; i < window_size; i++) {

			//int req_lat = gen_latency(mean, distribution_mode,0);
			//req_lat = req_lat >> 4;
            #if MEAS_GEN_LAT 
                printf("lat = %d \n",req_lat); 
            #endif
			//uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
			//uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;f

			#if debug 
				printf("lower %d; upper %d\n", lat_lower, lat_upper);
			#endif

			//conn->buf_send[i][1] = lat_lower;
			//conn->buf_send[i][0] = lat_upper;
			
			conn->buf_send[i][3] = (conn->ctx->qp->qp_num & 0xFF0000) >> 16;
			conn->buf_send[i][4] = (conn->ctx->qp->qp_num & 0x00FF00) >> 8;
			conn->buf_send[i][5] = (conn->ctx->qp->qp_num & 0x0000FF);

			int success = conn->pp_post_send(conn->ctx, /*remote_qp0*/ conn->dest_qpn, conn->size, i);
			if (success == EINVAL) printf("Invalid value provided in wr \n");
			else if (success == ENOMEM)	printf("Send Queue is full or not enough resources to complete this operation 1 \n");
			else if (success == EFAULT) printf("Invalid value provided in qp \n");
			else if (success != 0) {
				printf("success = %d, \n",success);
				fprintf(stderr, "Couldn't post send 2 \n");
				//return 1;
			}
			else {
				++conn->souts;

				#if debug 
					printf("send posted... souts = %d, \n",conn->souts);
				#endif
			}

			#if RR
				offset = ((SERVER_THREADS-1) & (offset+1));
				//offset++;// = (offset+1)%SERVER_THREADS;
				//if(offset == SERVER_THREADS) offset = 0;
				//offset = (offset+1)%SERVER_THREADS;
				conn->dest_qpn = remote_qp0+offset;
			#endif	

			#if RANDQP
				conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
			#endif
				
            //my_sleep(10, thread_num);
		}

		if (gettimeofday(&start, NULL)) {
			perror("gettimeofday");
		}								
		
		double prev_clock = now();
		while (conn->rcnt < conn->iters || conn->scnt < conn->iters) {
			if (terminate_load == 1) break;

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
						#if debug
							printf("T%d - send complete, a = %d, rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
						#endif
						break;

					case num_bufs ... 2*num_bufs-1:
						++conn->rcnt;
						--conn->routs;
						#if debug
							printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
						#endif
					
						//if(conn->rcnt % INTERVAL == 0) {
						//    double curr_clock = now();
						//    printf("T%d - %f rps, rcnt = %d, scnt = %d \n",thread_num,INTERVAL/(curr_clock-prev_clock),conn->rcnt,conn->scnt);
						//    prev_clock = curr_clock;
						//}
						//if(conn->rcnt > conn->iters - INTERVAL/1000000 && conn->rcnt % 1 == 0) printf("T%d - rcnt = %d, scnt = %d \n",thread_num,conn->rcnt,conn->scnt);


						if(!conn->pp_post_recv(conn->ctx, a, false)) ++conn->routs;
						if (conn->routs != window_size) fprintf(stderr,"Loading thread %d couldn't post receive (%d)\n", thread_num, conn->routs);

						//#if 0
						if(conn->scnt < conn->iters) {
							/*
							int req_lat = gen_latency(mean, distribution_mode,0);
							req_lat = req_lat >> 4;
							#if MEAS_GEN_LAT 
								printf("lat = %d \n",req_lat); 
							#endif
							uint lat_lower = req_lat & ((1u <<  8) - 1);//req_lat % 0x100;
							uint lat_upper = (req_lat >> 8) & ((1u <<  8) - 1);//req_lat / 0x100;
						
							#if debug 
								printf("lower %d; upper %d\n", lat_lower, lat_upper);
							#endif

							conn->buf_send[a-num_bufs][1] = lat_lower;
							conn->buf_send[a-num_bufs][0] = lat_upper;
							*/
							int success = conn->pp_post_send(conn->ctx, /*remote_qp0*/ conn->dest_qpn, conn->size, a-num_bufs);
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
								offset = ((SERVER_THREADS-1) & (offset+1));
								//offset++;// = (offset+1)%SERVER_THREADS;
								//if(offset == SERVER_THREADS) offset = 0;
								//offset = (offset+1)%SERVER_THREADS;
								conn->dest_qpn = remote_qp0+offset;
							#endif	

							#if RANDQP
								conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
							#endif
						}
						//#endif
					
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

		if (gettimeofday(&end, NULL)) {
			perror("gettimeofday");
		}
	} 

	float usec = (end.tv_sec - start.tv_sec) * 1000000 +(end.tv_usec - start.tv_usec);
	//long long bytes = (long long) conn->size * conn->iters * 2;

	if(thread_num == active_thread_num - 1){
		rps[thread_num] = conn->sync_iters/(usec/1000000.);
		printf("Meas. Thread: %d iters in %.5f seconds, rps = %f \n", conn->rcnt, usec/1000000., conn->rcnt/(usec/1000000.));
	}
	else if (thread_num < active_thread_num - 1) {
		rps[thread_num] = conn->rcnt/(usec/1000000.);
		printf("Thread %d: %d iters in %.5f seconds, rps = %f \n", thread_num, conn->rcnt, usec/1000000., rps[thread_num]);
	}

	// Dump out measurement results from measurement thread
	if(thread_num == active_thread_num - 1) {
		terminate_load = 1;
		printf("Measurement done, terminating loading threads\n");
        all_rcnts[thread_num] = conn->rcnt;
        all_scnts[thread_num] = conn->scnt;
	    ret = pthread_barrier_wait(&barrier);
		//sleep(5);
		double totalRPS = 0;
        uint32_t total_rcnt = 0;
        uint32_t total_scnt = 0;
		//aggregate RPS
		for(int i = 0; i < active_thread_num-1; i++){
    		printf("rps = %d \n",(int)rps[i]);
			totalRPS += rps[i];
		}
		for(int i = 0; i < active_thread_num; i++){
            total_rcnt += all_rcnts[i];
            total_scnt += all_scnts[i];
		}
		//printf("avgRPS = %f \n",totalRPS/active_thread_num-1);
		printf("total RPS = %d, total rcnt = %d, total scnt = %d \n", (int)totalRPS,(int)total_rcnt,(int)total_scnt) ;
		sleep(10);
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

		const int num_bufs = conn->sync_bufs_num;
        for (int i = 0; i < num_bufs; i++) {
            //send one pkt with 0xFFFF
			conn->buf_send[i][1] = 0;
			conn->buf_send[i][0] = 0;
            conn->buf_send[i][2] = 255;
            conn->buf_send[i][3] = 255;

			int success = conn->pp_post_send(conn->ctx, conn->dest_qpn /*conn->rem_dest->qpn*/, conn->size, i);
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
				offset = ((SERVER_THREADS-1) & (offset+1));
				//offset++;// = (offset+1)%SERVER_THREADS;
				//if(offset == SERVER_THREADS) offset = 0;
				//offset = (offset+1)%SERVER_THREADS;
				conn->dest_qpn = remote_qp0+offset;
			#endif

			#if RANDQP
				conn->dest_qpn = remote_qp0+genRandDestQP(thread_num);
			#endif
		}

        //wait to receive pkt
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
        printf("%d\n", (int)totalRPS);
	}

    #if 1
	if (thread_num < active_thread_num - 1){
        //sleep(10);
        for (int u=0; u<1000000; u++) {
            const int num_bufs = conn->bufs_num;

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
					    //#if debug
						    //printf("T%d - recv complete, a = %d, rcnt = %d , scnt = %d, routs = %d, souts = %d \n",thread_num,a,conn->rcnt,conn->scnt,conn->routs,conn->souts);
					    //#endif
                }        
            }
        }
        all_rcnts[thread_num] = conn->rcnt;
        all_scnts[thread_num] = conn->scnt;
	    ret = pthread_barrier_wait(&barrier);
    }

    if(conn->rcnt != conn->scnt) printf("\n T%d DID NOT DRAIN! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, conn->rcnt,conn->scnt,conn->routs,conn->souts);
    else printf("\n T%d DRAINED! - rcnt = %d, scnt = %d, routs = %d, souts = %d  \n",thread_num, conn->rcnt,conn->scnt,conn->routs,conn->souts);
    #endif
	
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
	while ((c = getopt (argc, argv, "w:t:d:g:v:q:m:s:r:p:c:")) != -1)
    switch (c)
	{
      case 'w':
        window_size = atoi(optarg);
        break;
      case 't':
        active_thread_num = atoi(optarg)+1; //adding one for the measurement thread
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
      default:
	  	printf("Unrecognized command line argument\n");
        return 0;
    }

	printf("window size is %d; thread number is %d; output dir is %s; devname is %s, gidx is %d, mode is %d, server ip = %s, ratio = %d, percent = %d \n", window_size, active_thread_num, output_dir, ib_devname_in, gidx_in, distribution_mode, servername, bimodal_ratio,  long_query_percent);

	assert(active_thread_num >= 1);

    rps = (double*)malloc((active_thread_num-1)*sizeof(double));
    all_rcnts = (uint32_t*)malloc(active_thread_num*sizeof(uint32_t));
    all_scnts = (uint32_t*)malloc(active_thread_num*sizeof(uint32_t));

	vector<RDMAConnection *> connections;
	for (int i = 0; i < active_thread_num; i++) {
		RDMAConnection *conn;
		connections.push_back(conn);
	}

	ret = pthread_barrier_init(&barrier, &attr, active_thread_num);
	assert(ret == 0);

  	struct thread_data tdata [connections.size()];
	pthread_t pt[active_thread_num];
	for(int i = 0; i < active_thread_num; i++){
		tdata[i].conn = connections[i];  
		tdata[i].id = i;
		int ret = pthread_create(&pt[i], NULL, client_threadfunc, &tdata[i]);
		assert(ret == 0);
		if(i == 0) sleep(3);
	}

	for(int i = 0; i < active_thread_num; i++){
		int ret = pthread_join(pt[i], NULL);
		assert(!ret);
	}
	

}
