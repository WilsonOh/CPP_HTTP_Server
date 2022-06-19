CC=g++
CFLAGS=@compile_flags.txt

main: main.cpp HttpServer.cpp
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -f HttpServer
