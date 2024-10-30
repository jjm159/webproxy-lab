#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


void doit(void *vargp);
int is_valid_command_line(int argc);
void parse_uri(char *uri, char *hostname, char *port);
int read_request_from_client(int clientfd, char *request, char *hostname, char *port);
void send_request_to_server_and_send_to_client_with_server_response(int clientfd, char *request, char *response, char *hostname, char *port);

void parse_hostname_port(const char *uri, char *hostname, char *port);
void parse_path(const char *uri, char *path);


int main(int argc, char **argv) {
  
  if (!is_valid_command_line(argc)) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  pthread_t tid;

  struct sockaddr_storage client_addr;
  int listenfd = Open_listenfd(argv[1]);

  printf("🍎 실행\n");

  while (1)
  {
    socklen_t client_addr_len = sizeof(client_addr);

    int connfd = Accept(listenfd, (SA *)&client_addr, &client_addr_len);
    
    char hostname[MAXLINE];
    char port[MAXLINE];
    Getnameinfo((SA*)&client_addr, client_addr_len, hostname, MAXLINE, port, MAXLINE, 0);

    printf("🍎 Accepted connection from (%s, %s)---------------------\n", hostname, port);

    Pthread_create(&tid, NULL, doit, &connfd);
    
    printf("🍎 Closed connection from (%s, %s)---------------------\n", hostname, port);
  }

  return 0;
}

void doit(void *vargp)
{

  Pthread_detach(pthread_self());

  int clientfd = *(int *)vargp;

  char hostname[MAXLINE], port[MAXLINE];

  char request_from_client[MAXLINE] = { NULL };
  int result = read_request_from_client(clientfd, request_from_client, hostname, port);
  if (result == -1) {
    printf("request empty\n");
    Close(clientfd);
    return;
  }

  printf("# result of client request parse\n%s\n", request_from_client);

  char response_from_server[MAXLINE] = { NULL };
  send_request_to_server_and_send_to_client_with_server_response(clientfd, request_from_client, response_from_server, hostname, port);

  printf("# result of server response parse\n%s\n", response_from_server);

  Close(clientfd);
}

void send_request_to_server_and_send_to_client_with_server_response(int clientfd, char *request, char *response, char *hostname, char *port) 
{
  printf("# Let's send request to server\n");

  char method[MAXLINE];
  char uri[MAXLINE];

  sscanf(request, "%s %s %*s", method, uri);
  printf("method, uri: %s %s\n", method, uri);
  printf("hostname: %s, port: %s\n", hostname, port);

  // connect server
  int serverfd = Open_clientfd(hostname, port);

  // write request
  printf("%s", request);

  Rio_writen(serverfd, request, strlen(request));

  printf("write to server fd 완료\n");

  // read response
  rio_t rio;
  Rio_readinitb(&rio, serverfd);

  printf("read from server fd 완료\n");

  char buf[MAXLINE];
  size_t count = 0;
  while ((count = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
  {
      Rio_writen(clientfd, buf, count);
      // 추가로 response에 담기?
  }
  
  Close(serverfd);

  return;
}

int read_request_from_client(int clientfd, char *request, char *hostname, char *port)
{
  int requestIdx = 0;

  struct stat sbuf;

  char buf[MAXLINE];
  char method[MAXLINE];
  char uri[MAXLINE];
  char version[MAXLINE];

  // Read Network File
  rio_t rio;
  Rio_readinitb(&rio, clientfd);
  Rio_readlineb(&rio, buf, MAXLINE);

  if (!strcmp(buf, "\r\n"))
  {
    return -1;
  }

  printf("🍎 %d\n", strcmp(buf, "\n"));

  // Request Start Line
  printf("# Request Start Line:\n");

  printf("%s", buf);

  sscanf(buf, "%s %s %s", method, uri, version);

  char path[MAXLINE];
  parse_path(uri, path);

  requestIdx += sprintf(request + requestIdx, "%s %s %s\n", method, path, "HTTP/1.0");

  // Request Headers
  printf("# Request headers:\n");

  int found_host = 0;

  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("%s", buf);

    char key[50];
    char *pos;

    pos = strstr(buf, ": ");
    if (pos != NULL)
    {
      strncpy(key, buf, pos - buf);
      key[pos - buf] = '\0';
    }

    if (!strcasecmp(key, "Host"))
    {

      char *host_start = strstr(buf, "Host: ");
      char *host;

      if (host_start != NULL)
      {
        host_start += strlen("Host: ");           // "Host: " 이후로 포인터 이동
        size_t len = strcspn(host_start, "\r\n"); // "\r\n" 전까지 길이 계산

        // 필요한 부분만 복사하기 위해 len + 1 크기로 메모리 할당
        host = malloc(len + 1);
        if (host == NULL)
        {
          fprintf(stderr, "Memory allocation failed\n");
          return 1;
        }

        // IP 주소와 포트 부분만 복사
        strncpy(host, host_start, len);
        host[len] = '\0'; // 널 종료 문자 추가

        // 결과 출력
        printf("Extracted Host: %s\n", host);

        parse_hostname_port(host, hostname, port);
        printf("💩 end parse uri: %s %s👍\n", hostname, port);

        // 메모리 해제
        free(host);
      }
      else
      {
        printf("Host not found\n");
      }
    }
    else if (!strcasecmp(key, "User-Agent"))
    {
      continue;
    }
    else if (!strcasecmp(key, "connection"))
    {
      continue;
    }
    else if (!strcasecmp(key, "Proxy-Connection"))
    {
      continue;
    }

    // end 처리는  while 밖에서
    if (strcmp(buf, "\r\n"))
    {
      requestIdx += sprintf(request + requestIdx, buf);
    }
  }

  requestIdx += sprintf(request + requestIdx, user_agent_hdr);
  requestIdx += sprintf(request + requestIdx, "Connection: close\r\n");
  requestIdx += sprintf(request + requestIdx, "Proxy-Connection: close\r\n");

  // header end 처리
  requestIdx += sprintf(request + requestIdx, "\r\n");

  // get 요청만 처리 - body는 고려 X

  return requestIdx;
}

int is_valid_command_line(int argc)
{
  return argc < 2 ? 0 : 1;
}

void parse_hostname_port(const char *uri, char *hostname, char *port)
{
  const char *url_ptr = uri;

  if (strncmp(uri, "https://", 8) == 0)
  {
    url_ptr += 8; // "https://" 뒤로 포인터 이동
  }
  else if (strncmp(uri, "http://", 7) == 0)
  {
    url_ptr += 7; // "http://" 뒤로 포인터 이동
  }

  const char *colon_pos = strchr(url_ptr, ':');
  const char *slash_pos = strchr(url_ptr, '/');

  if (colon_pos)
  {
    // 호스트네임 추출
    strncpy(hostname, url_ptr, colon_pos - url_ptr);
    hostname[colon_pos - url_ptr] = '\0'; // 널 종료 문자 추가

    // 포트 추출
    if (slash_pos)
    {
      strncpy(port, colon_pos + 1, slash_pos - colon_pos - 1);
      port[slash_pos - colon_pos - 1] = '\0'; // 널 종료 문자 추가
    }
    else
    {
      strcpy(port, colon_pos + 1); // 슬래시가 없으면 남은 문자열을 포트로
    }
  }
}

void parse_path(const char *uri, char *path)
{
  const char *url_ptr = uri;

  if (strncmp(uri, "https://", 8) == 0)
  {
    url_ptr += 8; // "https://" 뒤로 포인터 이동
  }
  else if (strncmp(uri, "http://", 7) == 0)
  {
    url_ptr += 7; // "http://" 뒤로 포인터 이동
  }
  const char *slash_pos = strchr(url_ptr, '/');

  // 경로 추출
  if (slash_pos)
  {
    strcpy(path, slash_pos); // 슬래시 이후 전체를 경로로 복사
  }
  else
  {
    strcpy(path, "/"); // 기본 경로는 "/"
  }
}