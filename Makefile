build:
	g++ -g server.cpp chunk.cpp constants.h -o server.bin -lpthread
	g++ -g client.cpp chunk.cpp constants.h -o client.bin -lpthread