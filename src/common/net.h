#ifndef PQC_NET_H
#define PQC_NET_H
/* Minimal TCP helpers. Numeric IPv4 only (no DNS/NSS): a non-numeric host
 * is rejected with -1, which keeps fully-static binaries free of NSS. */

int tcp_listen(const char *ip, int port);               /* listen fd, or -1 */
int tcp_accept(int listen_fd, int timeout_ms);           /* conn fd, or -1   */
int tcp_connect(const char *ip, int port, int timeout_ms); /* conn fd, or -1 */
int tcp_port(int fd);         /* local port bound to fd (host order), or -1  */

#endif /* PQC_NET_H */
