CC=g++
CFLAGS=@compile_flags.txt

HttpServer: HttpServer.cpp strutil.hpp get_ip.hpp
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm -f HttpServer
