#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

#define PORTA 50000
#define BUFFER_SIZE 4096
#define RAIZ "./www"

char *decodificarEspacos(const char *caminhoCodificado) {
    if (!caminhoCodificado) return NULL;

    size_t len = strlen(caminhoCodificado);
    char *resultado = malloc(len + 1);
    if (!resultado) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (caminhoCodificado[i] == '%' && i + 2 < len &&
            caminhoCodificado[i+1] == '2' && caminhoCodificado[i+2] == '0') {
            resultado[j++] = ' ';
            i += 2;
        } else {
            resultado[j++] = caminhoCodificado[i];
        }
    }

    resultado[j] = '\0';
    return resultado;
}


char *DirList(void) {
    const char *pasta = RAIZ;
    DIR *dir = opendir(pasta);
    if (!dir){
        perror("opendir");
        return NULL;
    }

    size_t teto = BUFFER_SIZE;
    size_t len = 0;
    char *buffer = malloc(teto);
    if (!buffer) {
        closedir(dir);
        return NULL;
    }
    buffer[0] = '\0';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        size_t tam = len + strlen(entry->d_name) + 3; // \r\n\0
        if (tam > teto) {
            while (teto < tam) teto *= 2;
            char *tmp = realloc(buffer, teto);
            if (!tmp) {
                free(buffer);
                closedir(dir);
                return NULL;
            }
            buffer = tmp;
        }
        strcat(buffer, entry->d_name);
        strcat(buffer, "\r\n");
        len = strlen(buffer);
    }

    closedir(dir);
    return buffer;
}


int enviar_arquivo(int cliente, char *path) {

    FILE *file = fopen(path, "rb");
    if (!file) {
        const char *err500 =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>500 - Erro interno</h1>";
        send(cliente, err500, strlen(err500), 0);
        close(cliente);

        return -1;
    }

    const char *ext = strrchr(path, '.');
    char tipo[64] = "application/octet-stream"; // padrão
    if (ext && strcmp(ext, ".html") == 0)
        strcpy(tipo, "text/html");
    else if (ext && strcmp(ext, ".jpg") == 0)
        strcpy(tipo, "image/jpeg");
    else if (ext && strcmp(ext, ".png") == 0)
        strcpy(tipo, "image/png");
    else if (ext && strcmp(ext, ".css") == 0)
        strcpy(tipo, "text/css");



    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n\r\n", tipo);

    if (hlen < 0) { // erro na escrita do cabeçlho
        fclose(file);
        return -1;
    }

    if (send(cliente, header, (size_t)hlen, 0) < 0) { // envia cabeça~ho primeiro
        perror("send header");
        fclose(file);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) { // manda todos os dados do arquivo
        ssize_t s = send(cliente, buffer, bytes, 0);
        if (s < 0) {
            perror("send content");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}


int main() {
    int servidor, cliente;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];

    servidor = socket(AF_INET, SOCK_STREAM, 0); // criação do descritor do socket
    if (servidor < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(servidor);
        exit(1);
    }

    memset(&addr, 0, sizeof(addr)); 
    addr.sin_family = AF_INET; // protocolo IPv4
    addr.sin_addr.s_addr = INADDR_ANY; // identifica o IP
    addr.sin_port = htons(PORTA); // PORT

    if(bind(servidor, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // associa IP e porta a um socket
        perror("bind");
        close(servidor);
        exit(1);
    }

    if (listen(servidor, 10) < 0) {
        perror("listen");
        close(servidor);
        exit(1);
    }

    printf("Servidor rodando em http://localhost:%d/\n", PORTA);

    while (1) {
        socklen_t len = sizeof(addr);
        cliente = accept(servidor, (struct sockaddr *)&addr, &len);
        if (cliente < 0) {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t rec = recv(cliente, buffer, sizeof(buffer) - 1, 0);
        if (rec <= 0) {
            close(cliente);
            continue;
        }

        char *metodo = strtok(buffer, " ");
        char *caminho = strtok(NULL, " ");
        if (!metodo || !caminho) {
            close(cliente);
            continue;
        }

        if (strcmp(metodo, "GET") != 0) {
            const char *not_allowed =
                "HTTP/1.1 405 Method Not Allowed\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>405 - Method Not Allowed</h1>";
            send(cliente, not_allowed, strlen(not_allowed), 0);
            close(cliente);
            continue;
        }

        if (strcmp(caminho, "/") == 0)
            caminho = "/index.html";

        if (strstr(caminho, "..")) {
            const char *resp =
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>403 - Acesso proibido</h1>";
            send(cliente, resp, strlen(resp), 0);
            close(cliente);
            continue;
        }

        char *caminho_decod = decodificarEspacos(caminho);
        
        // fprintf(stderr, "Caminho original: %s\n", caminho);
        // fprintf(stderr, "Caminho decodificado: %s\n", caminho_decod);
        
        char caminho_seguro[PATH_MAX];
        // Remove barra inicial se existir
        const char *caminho_sem_barra = caminho_decod;
        if (caminho_decod[0] == '/') {
            caminho_sem_barra++;
        }

        if (snprintf(caminho_seguro, sizeof(caminho_seguro), "%s/%s", RAIZ, caminho_sem_barra) >= (int)sizeof(caminho_seguro)) {
            // caminho muito longo 
            const char *resp =
            "HTTP/1.1 414 Request-URI Too Long\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>414 - Request-URI Too Long</h1>";
            send(cliente, resp, strlen(resp), 0);
            close(cliente);
            continue;
        }


        // transforma em caminho absoluto e verifica se está dentro de RAIZ 
        char caminho_real[PATH_MAX];
        fprintf(stderr, "Tentando resolver caminho: %s\n", caminho_seguro);
        
        if (!realpath(caminho_seguro, caminho_real)) {
            fprintf(stderr, "realpath falhou: %s\n", strerror(errno));
            // arquivo não existe -> enviar lista 
            char *lista = DirList();
            if (!lista) {
                const char *err500 =
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<h1>500 - Erro interno</h1>";
                send(cliente, err500, strlen(err500), 0);
                close(cliente);
                continue;
            }

            size_t resp_size = 1024 + strlen(lista);
            char *resp = malloc(resp_size);
            if (!resp) {
                free(lista);
                const char *err500 =
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<h1>500 - Erro interno</h1>";
                send(cliente, err500, strlen(err500), 0);
                close(cliente);
                continue;
            }

            int n = snprintf(resp, resp_size,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n\r\n"
                "<h1>Arquivos disponíveis:</h1>\r\n<pre>%s</pre>",
                lista);

            if (n < 0 || n >= (int)resp_size) {
                // erro
                free(lista);
                free(resp);
                close(cliente);
                continue;
            }
            
            // evia lista
            send(cliente, resp, (size_t)n, 0);
            free(lista);
            free(resp);
            close(cliente);
            continue;
        }

        char raiz_real[PATH_MAX];
        if (!realpath(RAIZ, raiz_real)) {
            perror("realpath raiz");
            close(cliente);
            continue;
        }

        // garantir que caminho_real começa com raiz_real 
        size_t rlen = strlen(raiz_real);
        if (strncmp(caminho_real, raiz_real, rlen) != 0 || (caminho_real[rlen] != '/' && caminho_real[rlen] != '\0')) {
            const char *forb =
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>403 - Acesso proibido</h1>";
            send(cliente, forb, strlen(forb), 0);
            close(cliente);
            continue;
        }


        // se chegou aqui, caminho_real é um arquivo 
        // fprintf(stderr, "passou pela verificação de arquivo\n\n");
        enviar_arquivo(cliente, caminho_real);
        
        close(cliente);
    }

    close(servidor);
    return 0;
}
