/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);
    n1 = atoi(strchr(arg1, '=') + 1);
    n2 = atoi(strchr(arg2, '=') + 1);
  }

  /* Make the response body */
  sprintf(content,
    "<html>\r\n"
    "<head>\r\n"
    "<style>\r\n"
    "body { font-family: Arial, sans-serif; background-color: #f0f0f0; text-align: center; padding: 30px; }\r\n"
    "h1 { color: #333333; }\r\n"
    "p { font-size: 18px; color: #555555; }\r\n"
    "img { border: 2px solid #333333; border-radius: 10px; }\r\n"
    "</style>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<h1>Welcome to add.com!</h1>\r\n"
    "<img width=\"640\" height=\"560\" src=\"/win.jpeg\">\r\n"
    "</img>\r\n"
    "<p>The answer is: %d + %d = %d</p>\r\n"
    "<p>Thanks for visiting!</p>\r\n"
    "</body>\r\n"
    "</html>\r\n", n1, n2, n1 + n2);

  /* Generate the HTTP response */
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
