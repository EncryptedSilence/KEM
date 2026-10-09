#include "net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

static int set_nonblock(int fd, int on) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0) return -1;
  fl = on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK);
  return fcntl(fd, F_SETFL, fl);
}

/* Numeric IPv4 only: inet_pton returns 1 on a valid dotted-quad, 0 otherwise. */
static int make_addr(const char *ip, int port, struct sockaddr_in *sa) {
  memset(sa, 0, sizeof(*sa));
  sa->sin_family = AF_INET;
  sa->sin_port = htons((uint16_t)port);
  return inet_pton(AF_INET, ip, &sa->sin_addr) == 1 ? 0 : -1;
}

int tcp_listen(const char *ip, int port) {
  struct sockaddr_in sa;
  if (make_addr(ip, port, &sa) != 0) return -1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return -1; }
  if (listen(fd, 16) != 0) { close(fd); return -1; }
  return fd;
}

int tcp_port(int fd) {
  struct sockaddr_in sa;
  socklen_t len = sizeof(sa);
  if (getsockname(fd, (struct sockaddr *)&sa, &len) != 0) return -1;
  return ntohs(sa.sin_port);
}

int tcp_accept(int listen_fd, int timeout_ms) {
  struct pollfd pfd = { .fd = listen_fd, .events = POLLIN };
  int r = poll(&pfd, 1, timeout_ms);
  if (r <= 0) return -1;                 /* timeout (0) or error (<0) */
  int fd = accept(listen_fd, NULL, NULL);
  if (fd < 0) return -1;
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  return fd;
}

int tcp_connect(const char *ip, int port, int timeout_ms) {
  struct sockaddr_in sa;
  if (make_addr(ip, port, &sa) != 0) return -1;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  if (set_nonblock(fd, 1) != 0) { close(fd); return -1; }

  int rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
  if (rc != 0 && errno != EINPROGRESS) { close(fd); return -1; }
  if (rc != 0) {
    struct pollfd pfd = { .fd = fd, .events = POLLOUT };
    int r = poll(&pfd, 1, timeout_ms);
    if (r <= 0) { close(fd); return -1; }
    int err = 0;
    socklen_t el = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0 || err != 0) {
      close(fd); return -1;
    }
  }
  set_nonblock(fd, 0);
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  return fd;
}
