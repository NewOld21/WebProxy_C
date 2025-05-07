/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

// 클라이언트의 요청을 처리하는 메인 함수
void doit(int fd) 
{
  
  int is_static; // 요청이 동적 or 정적
  struct stat sbuf;  // 파일의 메타데이터를 저장할 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; //요청 헤더 본문  저장 // 요청 라인의 각 항목 저장용 [method, uri, version] ex) GET / HTTP/1.0
  char filename[MAXLINE], cgiargs[MAXLINE]; // URL를 변환한 실제 파일 경로 // CGI 인자값 저장용
  rio_t rio; // Robust I/O를 위한 버퍼링 구조체(통신을 위한 구조체)

  Rio_readinitb(&rio, fd); // Rio를 사용하기 위해 초기화(rio와 fd를 연결)
  Rio_readlineb(&rio, buf, MAXLINE); // 첫 요청 라인을 읽어 buf에 저장 ex) GET /index.html HTTP/1.0
  printf("Request headers:\n"); 
  printf("%s", buf); // 디버깅 헤더 출력
  sscanf(buf, "%s %s %s" , method, uri, version); // 첫 요청 라인에서 method, URI, HTTP버전 추출 (method = GET, URI = /index.html, HTTP V = HTTP/1.0)
  



  // strcasecmp(method, "GET") 대소문자 상관없이 method == GET ? 확인 아닐 경우 clienterror(501) 에러  리턴
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){ 
    clienterror(fd, method, "501",  "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); // 나머지 요청 헤더 읽기 

  is_static = parse_uri(uri, filename, cgiargs); //parse_uri함수를 활용해 요청이 동적인지 정적인지 확인 (1 = 정적, 0 = 동적)
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404",  "Not found", "Tiny counld't find this file");
    return;
  }

  if(!strcasecmp(method, "HEAD")){
    serve_head(fd, filename, sbuf.st_size);
    return;
  }


  // 요청이 정적일 때
  if(is_static) {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403",  "Forbidden", "Tiny counld't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  // 요청이 동적일 때
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403",  "Forbidden", "Tiny counld't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char*errnum, char*shortmsg, char*longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body,"<html><title>Tiny Error</title>");
  sprintf(body + strlen(body),"<body bgcolor=""ffffff"">\r\n" );
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
  sprintf(body + strlen(body), "<p>%s: %s\r\n",  longmsg, cause);
  sprintf(body + strlen(body),"<hr><em>The Tiny Web server</em>\r\n");

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  sprintf(buf + strlen(buf), "Content-type: text/html\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));  // 헤더 한 번에 전송
  Rio_writen(fd, body, strlen(body)); // body 전송

}

// 남은 요청 헤더 읽고 츨력 후 버리기
// 요청 본문(body)으로 넘어가기 위해 헤더 부분을 전부 읽고 소모하는 역할
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE]; // 

  Rio_readlineb(rp, buf, MAXLINE); // 첫 줄 읽기
  while(strcmp(buf, "\r\n")){ // 빈줄이 나올 때까지(body 전까지)
    Rio_readlineb(rp, buf, MAXLINE); // 한 줄씩 읽기
    printf("%s", buf); // 한 줄씩 출력
  }

  return;
}

int parse_uri(char *uri, char *filename, char*cgiargs){
  char *ptr;
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");
    strcpy(filename,".");
    strcat(filename, uri);
    if(uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;
  }
  else{
    ptr = index(uri,'?');
    if(ptr){
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else{
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
      
  }
}

void serve_head(int fd, char *filename, int filesize){
  char filetype[MAXLINE], buf[MAXBUF];
  /* send response headers to client*/
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf , "%sserver: Tiny Web Server\r\n", buf);
  sprintf(buf , "%sConnection: close\r\n", buf);
  sprintf(buf , "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf , "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));

}


void serve_static(int fd, char *filename, int filesize) //Tiny serve_static은 정적 컨텐츠를 클라이언트에게 서비스한다.
{
  int srcfd; // 파열 열 때 fd
  char *srcp, filetype[MAXLINE], buf[MAXBUF]; // srcp는 파일 내용을 담을 곳 // filetype는 파일 타입 저장 // buf는 서버가 보내줄 글자를 저장하는 곳

  // 파일 타입 가지고 오기 
  get_filetype(filename, filetype);
  // 서버와 연결 성공 => 응답 헤더 전달
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf , "%sserver: Tiny Web Server\r\n", buf);
  sprintf(buf , "%sConnection: close\r\n", buf);
  sprintf(buf , "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf , "%sContent-type: %s\r\n\r\n", buf, filetype);

  // 응답 헤더 클라이언트에 보내기
  Rio_writen(fd, buf, strlen(buf));


  printf("Response headers:\n");
  printf("%s", buf);

  // 요청 받은 파일 열어 파일 디스크립트 받기
  srcfd = Open(filename, O_RDONLY, 0);

  // mmap() 를 사용하면 더 편하게 가능 (Malloc + Rio_readn)
  // srcp = Mmap(0, filesize, PR0T_READ, MAP_PRIVATE, srcfd, 0);

  // 파일 크기만큼 메모리 준비
  srcp = (char*)Malloc(filesize);
  // 파일 내용을 메모리에 다 읽기
  Rio_readn(srcfd, srcp, filesize);
  // 커널에게 파일 디스크립터 반납
  Close(srcfd);
  // 읽은 파일 클라이언트에 전달
  Rio_writen(fd, srcp, filesize);
  // 할당한 srcp 반환
  free(srcp);

}

// filetype을 MIME 방식으로 변경
void get_filetype(char *filename, char *filetype)
{
  if(strstr(filename, ".html"))
  {
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif"))
  {
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png"))
  {
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg"))
  {
    strcpy(filetype, "image/jpg");
  }
  else if (strstr(filename, ".jpeg"))
  {
    strcpy(filetype, "image/jpeg");
  }
  else if (strstr(filename, ".mp4"))
  {
    strcpy(filetype, "video/mp4");
  }
  else if (strstr(filename, ".mpg"))
  {
    strcpy(filetype, "video/mpeg");
  }
  else
  {
    strcpy(filetype, "text/plain");
  }
}

// CGI 프로그램을 실행해 동적 컨텐츠를 처리하는 함수
// HTTP 헤더 전송 → fork()로 자식 프로세스 생성 → CGI 프로그램 실행
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist [] = { NULL }; // buf는 문서 저장 공간 // emptylist는 실행 시 옵션 목록
 
  // 서버와 연결 성공 => 응답 헤더 전달
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 생성
  if(Fork() == 0) { 
  
    setenv("QUERY_STRING", cgiargs, 1); // 쿼리 정보 CGI 환경 변수 등록
    Dup2(fd, STDOUT_FILENO);  // stdout -> fd로 변경(인터넷으로 보내는 통로(fd)로)
    Execve(filename, emptylist, environ); // CGI 프로그램 실행
  }
  Wait(NULL); /*parent waits for and reaps child */
}

