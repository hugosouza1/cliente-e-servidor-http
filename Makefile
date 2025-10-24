build_cliente:
	gcc clienteHTTP.c -o cliente
	
build_server:
	gcc servidorHTTP.c -o server

server: build_server
	./server

cliente: build_cliente
	./cliente http://localhost:50000/_MetaHeursticas__Trabalho_1.pdf

