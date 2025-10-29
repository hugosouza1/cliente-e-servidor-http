URL ?= http://localhost:50000/a o.pdf

build_cliente:
	gcc clienteHTTP.c -o cliente.exe
	
build_server:
	gcc servidorHTTP.c -o server.exe

server: build_server
	./server.exe

cliente: build_cliente
	./cliente.exe ${URL}

clean:
	rm *.exe