build_cliente:
	gcc clienteHTTP.c -o cliente.exe
	
build_server:
	gcc servidorHTTP.c -o server.exe

server: build_server
	./server.exe

cliente: build_cliente
	./cliente.exe http://localhost:50000/_MetaHeursticas__Trabalho_1.pdf

clean:
	rm *.exe