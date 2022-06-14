RDMA MICROBENCHMARK README
NOTE: 
TO DO - Add command line arguments.
Currently, all parameters will require a manual change in the code and recompilation.

TO COMPILE: make
FIRST START THE SERVER: ./UD_Server -s [client IP addr] -t [thread_num] -g [gidx] -v [ib_devname]

Look at the first [server_qpn] in server output. Example:

    local address:  LID 0x0000, QPN 0x001d40, (int)QPN 7488, PSN 0x000000: GID ::ffff:192.168.1.2

Here [server_qpn]=7488

TO DO A SINGLE CLIENT RUN: 

    ./UD_Client -w [window_size] -t [thread_num] -d [output_dir] -v [ib_devname] -g [gidx] -q [server_qpn] -m [distribution_mode] -s [server IP addr] -c [num server cores]

[distribution_mode] should be a number: 0 - FIXED, 1 - NORMAL, 2 - UNIFORM, 3 - EXPONENTIAL.

TO SWEEP WINDOW_SIZE/THREAD_NUM: 

    ./run_client.sh [output_dir] [server_qpn] [distribution_mode] [server_ip_addr]

[distribution_mode] should be one of FIXED, NORMAL, UNIFORM, EXPONENTIAL.

HOW AND WHERE TO CHANGE RUNTIME PARAMETERS:

1. Number of server threads
    Find NUM_THREADS macro in ud_pingpong_server.cc and SERVER_THREADS macro in ud_pingpong_client.cc.
    These two values MUST be the same.

2. Number of client threads
    The maximum number of client threads is the NUM_THREADS macro in ud_pingpong_client.cc.
    n is the number of loading threads and 1 is for the measurement thread (n+1 threads total) where n can be any value from 1 to number_of_cores -1

    The number of loading threads is defined in ud_pingpong_client.cc:

        int active_thread_num = 1;

    active_thread_num is overwritten by the -t parameter input at the beginning of each run.


3. Window Size (both client and server)
    Find the following two variables in 
    ServerRDMAConnection.h 

        unsigned int rx_depth = 64+1;
        static const int recv_bufs_num = 64+1;

    and ClientRDMAConnection.h

        unsigned int rx_depth = 64;
        static const int recv_bufs_num = 64;

    In the example above, 64 is the working window size for server threads and the maximum window size for client threads.
    The window size for server threads must always be at least one extra to account for the latency measuring client thread.

    The working window size on the client thread is defined in ud_pingpong_client.cc:

        int window_size = 1;

    window_size is overwritten by the -w parameter input at the beginning of each run.


4. Loading Thread Iterations (both client and server)
    The server stays active until killed and doesn't have a maximum number of iterations.
    Maximum number of interations per client is defined by variable in ClientRDMAConnection.h

        long long int iters = 100000000;


5. Measurement Thread Iterations (client only)
    Find the following variable in ClientRDMAConnection.h

        long long int sync_iters = 1000000;

    After the measurement thread reaches sync_iters, it kills all other loading threads and dump the recorded latency results.


REPORTING RPS (requests per second)

    Each client thread reports its RPS after completing 10M iterations.
    The average RPS per thread as well as average RPS across all threads is also reported at the end of each run.
