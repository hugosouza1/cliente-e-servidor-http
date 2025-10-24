#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

typedef struct {
    char* url_completa;
    char* host;
    char* port;
    char* arquivo;
} Dados;

void freeDados(Dados* URl){
    if(!URl) return;
    free(URl->url_completa);
    free(URl->host);
    free(URl->port);
    free(URl->arquivo);
    free(URl);
}

Dados* parse(char* entrda){
    if(entrda == NULL) return NULL;

    Dados* URl = (Dados*) malloc (sizeof(Dados));
    if(!URl) return NULL;

    URl->url_completa = strdup(entrda);
    if(!URl->url_completa) { free(URl); return NULL; }

    char* token;
    char* saveptr1;

    token = strstr(URl->url_completa, "://");
    if(token != NULL){
        token += 3;
    } else {
        token = URl->url_completa;
    }

    char* saveptr2;
    char* hostport = strtok_r(token, "/", &saveptr1); 
    char* caminho  = strtok_r(NULL, "", &saveptr1);   

    if(!hostport){
        freeDados(URl);
        return NULL;
    }

    char* temp = strdup(hostport);
    if(!temp){ freeDados(URl); return NULL; }

    char* p = strtok_r(temp, ":", &saveptr2);
    if(!p){ free(temp); freeDados(URl); return NULL; }
    URl->host = strdup(p);

    p = strtok_r(NULL, ":", &saveptr2);
    if(p != NULL){
        URl->port = strdup(p);
    } else {
        URl->port = strdup("80");
    }
    free(temp);

    if(caminho != NULL && strlen(caminho) > 0){
        size_t len = strlen(caminho) + 2;
        URl->arquivo = (char*) malloc(len);
        
        if(!URl->arquivo){ freeDados(URl); return NULL; }

        sprintf(URl->arquivo, "/%s", caminho);
    } else {
        URl->arquivo = strdup("/");
    }

    return URl;
}

int conexao_socket(Dados* URl){
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0; 

    int getInfo = getaddrinfo(URl->host, URl->port, &hints, &res);
    if (getInfo != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getInfo));
        return -1;
    }

    int sfd = -1;

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            // não conseguiu conectar
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // conectado 
            break;
        }

        // connect falhou 
        perror("connect");
        close(sfd);
        sfd = -1;
    }

    freeaddrinfo(res);

    if (sfd == -1) {
        fprintf(stderr, "Não foi possível conectar a %s:%s\n", URl->host, URl->port);
    }
    return sfd;
}

// garante que a requisição seja mandada toda
int send_all(int sfd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sfd, buf + total, len - total, 0); // ss para pegar negativo(codigo de erro)
        if (sent < 0) {
            if (errno == EINTR) continue; // interrompido por sinal, tenta de novo
            perror("send");
            return -1;
        }
        total += (size_t) sent;
    }
    return 0;
}

int get_http(Dados *URL){
    int sfd = conexao_socket(URL);
    if(sfd == -1) return -1;

    char request[1024];
    int r = snprintf(request, sizeof(request),
                     "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                     URL->arquivo, URL->host);
    if (r < 0 || r >= (int)sizeof(request)) {
        fprintf(stderr, "Erro ao montar request\n");
        close(sfd);
        return -1;
    }

    if (send_all(sfd, request, strlen(request)) < 0) {
        close(sfd);
        return -1;
    }
    char *nome = strrchr(URL->arquivo, '/');
    if (nome == NULL) nome = URL->arquivo; else nome++; // pula '/'
    if (*nome == '\0') nome = "index.html"; 

    const char *pastaDentino = "solicitados";
    char prefix[1024];
    snprintf(prefix, sizeof(prefix), "%s/", pastaDentino);
    size_t tam = strlen(prefix) + strlen(nome) + 1; // +1 para '\0'
    char *arquivo_saida = malloc(tam);
    if (!arquivo_saida) {
        perror("malloc");
        close(sfd);
        return -1;
    }
    snprintf(arquivo_saida, tam, "%s%s", prefix, nome);
    
    char buffer[4096];
    
     if (access(pastaDentino, F_OK) != 0){
        if (mkdir(pastaDentino, 777) != 0){
            perror("mkdir");
            close(sfd);
            return -1;
        }
     }

    FILE* f = fopen(arquivo_saida, "wb");
    if (!f) {
        perror("fopen");
        close(sfd);
        return -1;
    }

    int header_done = 0;
    ssize_t n;
    while (1) {
        n = recv(sfd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            if (!header_done) {
                char* body = NULL;
                size_t copy_len = (size_t)n;
                char *tmp = (char*) malloc(copy_len + 1);
                if(!tmp){ fclose(f); close(sfd); return -1;}

                memcpy(tmp, buffer, copy_len);
                tmp[copy_len] = '\0'; // para funcionar o strstr
                body = strstr(tmp, "\r\n\r\n");

                int status_code = 0;
                char* status_line = strstr(tmp, "HTTP/");
                if (status_line) {
                    sscanf(status_line, "HTTP/%*s %d", &status_code);
                    printf("Código HTTP: %d\n", status_code);

                    if (status_code >= 200 && status_code < 300) {
                        fprintf(stderr, "Sucesso\n");
                    } else if (status_code >= 300 && status_code < 400) {
                        fprintf(stderr, "Redirecionamento\n");
                    } else if (status_code == 404) {
                        fprintf(stderr, "Página não encontrada\n");
                    } else if (status_code >= 400 && status_code < 500) {
                        fprintf(stderr, "Erro do cliente (código %d)\n", status_code);
                    } else if (status_code >= 500) {
                        fprintf(stderr, "Erro do servidor (código %d)\n", status_code);
                    }
                }

                if (body) {
                    size_t header_prefix_len = (size_t)(body - tmp) + 4;
                    size_t body_bytes = (size_t)n - header_prefix_len;
                    fwrite(buffer + header_prefix_len, 1, body_bytes, f);
                    header_done = 1;
                }
                free(tmp);
            } else {
                fwrite(buffer, 1, (size_t)n, f); // ultima escrita
            }
        } else if (n == 0) {
            // conexão fechada 
            break;
        } else {
            if (errno == EINTR) continue;
            perror("recv");
            fclose(f);
            close(sfd);
            return -1;
        }
    }

    fclose(f);
    close(sfd);
    return 0;
}

int main(int argc, char *argv[]){
    if(argc < 2) {
        fprintf(stderr, "Argumento imcompleto\n");
        return 1;
    }

    Dados* URL = parse(argv[1]);
    if(!URL){
        fprintf(stderr, "Erro ao parsear URL\n");
        return 1;
    }

    if(get_http(URL) != 0){
        fprintf(stderr, "Falha ao obter HTTP\n");
        freeDados(URL);
        return 1;
    }

    printf("Arquivo salvo\n");
    freeDados(URL);
    return 0;
}
