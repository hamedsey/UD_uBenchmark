all:
	g++ ud_pingpong_client.cc -libverbs -Wall -lpthread -O3 -o UD_Client -std=gnu++11
	g++ ud_pingpong_server_reorder_fast.cc -libverbs -Wall -lpthread -O3 -o UD_Server -std=gnu++11 -mlzcnt -msse3 -mavx -DARCH_x86_64 -m64 -mavx512f -mavx512cd
	g++ ud_pingpong_middle.cc -libverbs -Wall -lpthread -O3 -o UD_Middle -std=gnu++11
clean:
	rm UD_Client UD_Server UD_Middle
