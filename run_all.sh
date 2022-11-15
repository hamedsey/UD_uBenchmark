#./run_client.sh RANDOM_fixed 23424 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh RANDOM_normal 23424 NORMAL 192.168.1.5 mlx5_1 3
#./run_client.sh RANDOM_uniform 23424 UNIFORM 192.168.1.5 mlx5_1 3
#./run_client.sh RANDOM_expo 23424 EXPONENTIAL 192.168.1.5 mlx5_1 3


#./run_client.sh bm_RANDOM 24768 BIMODAL 192.168.1.5 mlx5_1 3
#./run_client.sh bm_RR 23424 NORMAL 192.168.1.5 mlx5_1 3
#./run_client.sh bm_JSQ 23424 UNIFORM 192.168.1.5 mlx5_1 3
#./run_client.sh bm_JLQ 23424 EXPONENTIAL 192.168.1.5 mlx5_1 3


#//bimodal_ratios = [2, 5, 10, 25, 50, 100]
#//percents = [0.01, 0.05, 0.1, .25, .5]

#./run_client.sh fixed_RR 7872 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh expo_RR 8064 EXPONENTIAL 192.168.1.5 mlx5_1 3


#new

#./run_client.sh expo_RANDOM_5x 6912 EXPONENTIAL 192.168.1.5 mlx5_1 3
#./run_client.sh fixed_RANDOM_5x 6912 FIXED 192.168.1.5 mlx5_1 3
#./run_client_rev.sh fixed_JLQ_5x_rev 6528 FIXED 192.168.1.5 mlx5_1 3
#./run_client_rev.sh expo_JLQ_5x_rev 6528 EXPONENTIAL 192.168.1.5 mlx5_1 3
#./run_client_rev.sh fixed_RANDOM_5x_rev 6912 FIXED 192.168.1.5 mlx5_1 3
#./run_client_rev.sh expo_RANDOM_5x_rev 6912 EXPONENTIAL 192.168.1.5 mlx5_1 3

#!/usr/bin/env bash
#make
#i=1
qpn=$(echo $1)


#sudo /etc/init.d/openibd restart
#sudo ip route del 192.168.1.0/255.255.255.0 dev enp59s0f0
#sudo ip route add 192.168.1.5 dev enp59s0f0
#sudo ifconfig enp59s0f0 mtu 9000 up

#./run_client.sh RAND_fixed_2us $qpn FIXED 192.168.1.5 mlx5_0 3 12
#./run_client.sh RAND_expo_2us $qpn EXPONENTIAL 192.168.1.5 mlx5_0 3 12
#./run_client.sh 1c_bimodal_2us $qpn BIMODAL 192.168.1.5 mlx5_0 3 5 10 1
./run_client.sh 1c_bimodal_2us $qpn BIMODAL 192.168.1.5 mlx5_0 3 10 40 1

#./run_client.sh JLQ_expo_1us_16HT 16128 FIXED 192.168.1.5 mlx5_0 3
#./run_client.sh JLQ_expo_1us_16HT 16128 EXPONENTIAL 192.168.1.5 mlx5_0 3


#./run_client.sh r5_p10_RANDOM_new2 56256 BIMODAL 192.168.1.5 mlx5_1 3

#./run_client.sh echo1 57216 FIXED 192.168.1.5 mlx5_1 3

#./run_client.sh bm_JLQ_1x_2 1344 BIMODAL 192.168.1.5 mlx5_1 3



#./run_client.sh fixed_RR_no_state_2 768 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh fixed_RR_no_state_3 768 FIXED 192.168.1.5 mlx5_1 3


#./run_client.sh fixed_RR_new_4 17664 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh bm_RR 14976 BIMODAL 192.168.1.5 mlx5_1 3
#./run_client.sh expo_RR_new 15744 EXPONENTIAL 192.168.1.5 mlx5_1 3
#./run_client.sh normal_RR 15552 NORMAL 192.168.1.5 mlx5_1 3
#./run_client.sh uniform_RR 15552 UNIFORM 192.168.1.5 mlx5_1 3

#./run_client.sh fixed_JSQ_good_2 3456 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh fixed_JSQ_good_3 3456 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh fixed_RANDOM_2 1536 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh fixed_RANDOM_3 1536 FIXED 192.168.1.5 mlx5_1 3

#./run_client.sh bm_JSQ 14592 BIMODAL 192.168.1.5 mlx5_1 3
#./run_client.sh expo_JSQ 9024 EXPONENTIAL 192.168.1.5 mlx5_1 3
#./run_client.sh normal_JSQ 9024 NORMAL 192.168.1.5 mlx5_1 3
#./run_client.sh uniform_JSQ 9216 UNIFORM 192.168.1.5 mlx5_1 3


#./run_client.sh fixed_JLQ_clear_state 3840 FIXED 192.168.1.5 mlx5_1 3
#./run_client.sh bm_JLQ 13248 BIMODAL 192.168.1.5 mlx5_1 3
#./run_client.sh expo_JLQ 9024 EXPONENTIAL 192.168.1.5 mlx5_1 3
#./run_client.sh normal_JLQ 9024 NORMAL 192.168.1.5 mlx5_1 3
#./run_client.sh uniform_JLQ 9216 UNIFORM 192.168.1.5 mlx5_1 3
