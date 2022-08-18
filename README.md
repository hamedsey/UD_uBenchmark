Written by: Hamed Seyedroudbari (Arm Research - Summer 2022)

RDMA UD MICROBENCHMARK README
Fun fact: outperformed UCX's bandwidth perftest!

This RDMA microbenchmark was created using UD QPs to perform two tasks:

1. Generate load to test the packet processing limits of a DUT
2. Measure response latency

TO CLEAN: run "make clean"
TO COMPILE: run "make"

HOW TO RUN:

The microbenchmark can be run in two different ways:

1. (P2P) Client <-> Server
In this configuration, the client and server must be separate hosts/BlueField DPUs connected over the the network.

          NETWORK
CLIENT  < ------- >  SERVER

--------------                -------------- 
|            |     NETWORK    |	        |
|   CLIENT   |   <--------->  |   SERVER   |
|            |                |            |
--------------                --------------

First, start the server: 

     ./UD_Server -s [client IP addr] -t [thread_num] -g [gidx] -v [ib_devname]

Look at the first [server_qpn] in server output. Example:

    local address:  LID 0x0000, QPN 0x001d40, (int)QPN 7488, PSN 0x000000: GID ::ffff:192.168.1.2

Here [server_qpn] = 7488

Then, start the client: 

    ./UD_Client -w [window_size] -t [thread_num] -d [output_dir] -v [ib_devname] -g [gidx] -q [server_qpn] -m [distribution_mode] -s [server IP addr] -c [number of server cores]
    
NOTE: 
[thread_num] is the number of load generating thread. One latency measurement thread will be spawned in addition to the load generating threads.
[window_size] is the maximum number of outstanding requests per load generating client thread. the latency measurement thread is synchronous and has a window size of 1. keep in mind, enough buffers must be allocated in order to support varying window sizes. in order to change the number of buffers being allocated per run:

    Find the following two variables in <Client/Server/Middle>RDMAConnection.h 
    	unsigned int rx_depth = 64;
        static const int recv_bufs_num = 64;

    In the example above, 64 is the number of buffers allocated for the load generating client threads or server threads. The window size for server threads must always be at least one more than client threads to account for the latency measuring client thread.

[output_dir] must be created prior to starting client in order to save latency measurements and runtime data.
[distribution_mode] should be 0. Please only use 0.
By default, each client thread sends packets to server threads in a round-robin (RR) fashion starting from [server_qpn] to [server_qpn + number of server cores - 1].
the [server_qpn] is the value printed on the server's console (please see above).
The load generating threads stop after the latency measurement thread completes 1 million latency measurements.
[gidx] can be found by running the command "show_gids". i always use a value of 3 since it corresponds to using RoCEv2 (RoCE: RDMA over converged Ethernet) protocol.
[ib_devname] is the NIC port name that corresponds an IP address. you can see the port name, IP address it corresponds to, and its status by running "ibdev2netdev".


2. (Inline Bf-2 DPU) Client <-> BF-2 DPU <-> Server
In this configuration, the client sends packets to the BF-2's Arm cores in round-robin fashion. The Arm cores forward packets to the server cores in round-robin fashion. The server responds back to the BF-2 Arm core and the Bf-2 Arm core responds back to the original client thread.


--------------                --------------                --------------
|            |     NETWORK    |	 (MIDDLE)  |     NETWORK    |            |
|   CLIENT   |   <--------->  |  BF-2 DPU  |   <--------->  |   SERVER   |
|            |                |            |                |            |
--------------                --------------                --------------

First, start the server: 

     ./UD_Server -s [Middle Host IP addr] -t [thread_num] -g [gidx] -v [ib_devname]

Look at the first [server_qpn] in server output. Example:

    local address:  LID 0x0000, QPN 0x001d40, (int)QPN 7488, PSN 0x000000: GID ::ffff:192.168.1.2

    Here [server_qpn] = 7488. this value will be used when starting the Middle host.

Then, start the Middle host (usually the BlueField-2 DPU):

    ./UD_Middle -s [server IP addr] -k [client IP addr] -t [thread_num] -g [gidx] -v [ib_devname] -q [server_qpn] -c [number of server cores]

    [server_qpn] is the value printed on the server's console (please see above).


    when you run the above command, you will see an output like this:
        T5 - QPs = 7495 and 7496
        T6 - QPs = 7493 and 7499
        T4 - QPs = 7492 and 7500
        T2 - QPs = 7489 and 7498
        T1 - QPs = 7490 and 7497
        T7 - QPs = 7491 and 7502
        T0 - QPs = 7488 and 7501
        T3 - QPs = 7494 and 7503
    note that, each thread will create two QPs because the middle host is interfacing with two endpoints (the client and the server). look at the first QP number in front of "T0". in this case, it's 7488. this is the [middle_qpn] which will be used when starting the client. 

Then, start the client, as explained previously. The only difference here is the use of [middle_qpn] which in the example above is 7488.

    ./UD_Client -w [window_size] -t [thread_num] -d [output_dir] -v [ib_devname] -g [gidx] -q [middle_qpn] -m [distribution_mode] -s [server IP addr] -c [number of middle host cores]


REPORTING RPS (requests per second)
    The average RPS per thread as well as average RPS across all threads is also reported at the end of each run.
    The run terminates when the latency measurement thread has completed 1 million latency measurements.

At the end of each run, the client will display an output like this. it will tell you the total througput in RPS (requests per second). keep in mind that each request corresponds to a round trip, meaning there are two messages exchanged per completed request. therefore, the message rate is simple double the reported "RPS". the output will also report if each client thread drained. if a client thread does not drain, that means some packets were dropped. Packet drops will happen when there is insufficient bufering at the server.

example output from client after the end of a run: 

    total RPS = 4647220, total rcnt = 332513652, total scnt = 332513652

    T3 DRAINED! - rcnt = 85520982, scnt = 85520982, routs = 0, souts = 0

    T0 DRAINED! - rcnt = 81119819, scnt = 81119819, routs = 0, souts = 0

    T2 DRAINED! - rcnt = 82933176, scnt = 82933176, routs = 0, souts = 0

    T1 DRAINED! - rcnt = 81939675, scnt = 81939675, routs = 0, souts = 


For any questions, please feel free to contact me at hamed@gatech.edu
