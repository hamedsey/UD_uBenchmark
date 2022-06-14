all:
	g++ ud_pingpong_client.cc -libverbs -Wall -lpthread -O3 -o UD_Client -std=gnu++11
	g++ ud_pingpong_server.cc -libverbs -Wall -lpthread -O3 -o UD_Server
clean:
	rm UD_Client UD_Server
