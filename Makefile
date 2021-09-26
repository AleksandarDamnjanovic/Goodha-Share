build_linux:
	g++ -g server.cpp chunk.cpp constants.h -o server.bin -lpthread
	g++ -g client.cpp chunk.cpp constants.h -o client.bin -lpthread
	g++ -g detector.cpp constants.h -o detector.bin -lpthread

build_linux_final:
	g++ -O2 server.cpp chunk.cpp constants.h -o server.bin -lpthread
	g++ -O2 client.cpp chunk.cpp constants.h -o client.bin -lpthread
	g++ -O2 detector.cpp constants.h -o detector.bin -lpthread
	strip server.bin
	strip client.bin
	strip detector.bin

build_windows:
	g++ -g server_win.cpp chunk_win.cpp constants.h -o server.exe -lpthread -lws2_32
	g++ -g client_win.cpp chunk_win.cpp constants.h -o client.exe -lpthread -lws2_32
