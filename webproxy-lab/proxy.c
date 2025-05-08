#include "csapp.h"

void doit(int clientfd);
void parse_uri(char *uri, char *hostname, char *path, char *port);
int request_to_endserver(char *hostname, char* port, char *path, rio_t *client_rio);
void response_to_client(int clientfd, int serverfd);

int main(int argc, char **argv) {
    int listenfd, clientfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);

        doit(clientfd);
        Close(clientfd);
    }
    return 0;
}

void doit(int clientfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE], port[MAXLINE];
    int  serverfd;
    rio_t client_rio;

    Rio_readinitb(&client_rio, clientfd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        printf("Proxy only implements GET\n");
        return;
    }

    parse_uri(uri, hostname, path, port);

    serverfd = request_to_endserver(hostname, port, path, &client_rio);
    if (serverfd < 0) {
        printf("Request failed\n");
        return;
    }

    response_to_client(clientfd, serverfd);
}

void parse_uri(char *uri, char *hostname, char *path, char *port) {
    char *hostbegin, *hostend, *pathbegin;
    int len;

    strcpy(port, "80");   // 기본값

    hostbegin = strstr(uri, "//");
    if (hostbegin != NULL)
        hostbegin += 2;
    else
        hostbegin = uri;

    pathbegin = strchr(hostbegin, '/');
    if (pathbegin != NULL) {
        strcpy(path, pathbegin);
        len = pathbegin - hostbegin;
        strncpy(hostname, hostbegin, len);
        hostname[len] = '\0';
    } else {
        strcpy(hostname, hostbegin);
        strcpy(path, "/");
    }

    hostend = strchr(hostname, ':');
    if (hostend != NULL) {
        *hostend = '\0';
        strcpy(port, hostend + 1);  // 문자열 복사
    }
}

int request_to_endserver(char *hostname, char* port, char *path, rio_t *client_rio) {
    int serverfd;
    char buf[MAXLINE];

    serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        printf("Failed to connect to server\n");
        return -1;
    }

    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    Rio_writen(serverfd, buf, strlen(buf));

    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0)
            break;
        Rio_writen(serverfd, buf, strlen(buf));
    }

    sprintf(buf, "\r\n");
    Rio_writen(serverfd, buf, strlen(buf));

    return serverfd;
}

void response_to_client(int clientfd, int serverfd) {
    char buf[MAXLINE];
    rio_t server_rio;

    Rio_readinitb(&server_rio, serverfd);

    ssize_t n;
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n);
    }

    Close(serverfd);
}
