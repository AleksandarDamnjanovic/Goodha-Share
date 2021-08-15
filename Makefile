build_linux:
	g++ -g server.cpp chunk.cpp constants.h -o server.bin -lpthread
	g++ -g client.cpp chunk.cpp constants.h -o client.bin -lpthread

build_windows:
	g++ -g server_win.cpp chunk_win.cpp constants.h -o server.exe -lpthread -lws2_32
	g++ -g client_win.cpp chunk_win.cpp constants.h -o client.exe -lpthread -lws2_32
