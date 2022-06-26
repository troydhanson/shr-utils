#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "shr.h"

#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>

/* 
 *  shr-nsub
 *
 *  listen on a nanomsg pull socket, writes frame to shr ring
 *
 */

#define BATCH_FRAMES 10000
#define BATCH_MB     10
#define BATCH_BYTES  (BATCH_MB * 1024 * 1024)
char read_buffer[BATCH_BYTES];
struct iovec read_iov[BATCH_FRAMES];

struct {
  char *prog;
  int verbose;
  int epoll_fd;     /* epoll descriptor */
  int signal_fd;    /* to receive signals */
  char *file;       /* ring file name */
  struct shr *ring; /* open ring handle */
  char *buf;        /* buf for shr_readv */
  struct iovec *iov;/* iov for shr_readv */
  char *endpoint;   /* listening address */
  int sock;         /* nanomsg socket */
  int eid;          /* nanomsg endpoint */
  int nn_fd;        /* nanomsg poll fd */
} cfg = {
  .buf = read_buffer,
  .iov = read_iov,
  .epoll_fd = -1,
  .signal_fd = -1,
  .sock = -1,
};

/* signals that we'll accept via signalfd in epoll */
int sigs[] = {SIGHUP,SIGTERM,SIGINT,SIGQUIT,SIGALRM};

void usage() {
  fprintf(stderr,"usage: %s [options]\n", cfg.prog);
  fprintf(stderr,"options:\n"
                 "   -e <addr>  (listen on address e.g. tcp://0.0.0.0:3456)\n"
                 "   -r <RING>  (ring file name)\n"
                 "   -h         (this help)\n"
                 "   -v         (verbose)\n"
                 "\n");
  exit(-1);
}

int add_epoll(int events, int fd) {
  int rc;
  struct epoll_event ev;
  memset(&ev,0,sizeof(ev)); // placate valgrind
  ev.events = events;
  ev.data.fd= fd;
  if (cfg.verbose) fprintf(stderr,"adding fd %d to epoll\n", fd);
  rc = epoll_ctl(cfg.epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

int del_epoll(int fd) {
  int rc;
  struct epoll_event ev;
  rc = epoll_ctl(cfg.epoll_fd, EPOLL_CTL_DEL, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

/* work we do at 1hz  */
int periodic_work(void) {
  int rc = -1;

  rc = 0;

 done:
  return rc;
}

int handle_signal(void) {
  int rc=-1;
  struct signalfd_siginfo info;
  
  if (read(cfg.signal_fd, &info, sizeof(info)) != sizeof(info)) {
    fprintf(stderr,"failed to read signal fd buffer\n");
    goto done;
  }

  switch(info.ssi_signo) {
    case SIGALRM: 
      if (periodic_work() < 0) goto done;
      alarm(1); 
      break;
    default: 
      fprintf(stderr,"got signal %d\n", info.ssi_signo);  
      goto done;
      break;
  }

 rc = 0;

 done:
  return rc;
}

int handle_nnmsg(void) {
  int len, rc = -1;
  char *buf = NULL;
  ssize_t nr;

  // TODO DONTWAIT and loop til wouldblock
  // TODO alt, nano_recvmsg 
  len = nn_recv(cfg.sock, &buf, NN_MSG, 0);
  if (len < 0) {
    fprintf(stderr, "nn_recv: %s\n", nn_strerror(errno));
    goto done;
  }

  nr = shr_write(cfg.ring, buf, len);
  if (nr < 0) {
    fprintf(stderr, "shr_write: error\n");
    goto done;
  }

  rc = 0;

 done:
  if (buf) nn_freemsg(buf);
  return rc;
}

int main(int argc, char *argv[]) {
  char unit, *c, buf[100];
  int opt, rc=-1, n, ec;
  struct epoll_event ev;
  cfg.prog = argv[0];
  size_t optlen;

  while ( (opt = getopt(argc,argv,"v+hr:e:")) > 0) {
    switch(opt) {
      case 'v': cfg.verbose++; break;
      case 'r': cfg.file = strdup(optarg); break;
      case 'e': cfg.endpoint = strdup(optarg); break;
      case 'h': default: usage(); break;
    }
  }

  if (cfg.file == NULL) usage();
  if (cfg.endpoint == NULL) usage();

  /* open nanomsg pull socket */
  cfg.sock = nn_socket(AF_SP, NN_PULL);
  if (cfg.sock < 0) goto done;
  cfg.eid = nn_bind(cfg.sock, cfg.endpoint);
  if (cfg.eid < 0) goto done;
  
  /* block all signals. we accept signals via signal_fd */
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_SETMASK,&all,NULL);

  /* a few signals we'll accept via our signalfd */
  sigset_t sw;
  sigemptyset(&sw);
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaddset(&sw, sigs[n]);

  /* create the signalfd for receiving signals */
  cfg.signal_fd = signalfd(-1, &sw, 0);
  if (cfg.signal_fd == -1) {
    fprintf(stderr,"signalfd: %s\n", strerror(errno));
    goto done;
  }

  /* set up the epoll instance */
  cfg.epoll_fd = epoll_create(1); 
  if (cfg.epoll_fd == -1) {
    fprintf(stderr,"epoll: %s\n", strerror(errno));
    goto done;
  }

  // get nanomsg socket fd suitable for epoll
  optlen = sizeof(int);
  ec = nn_getsockopt(cfg.sock, NN_SOL_SOCKET, NN_RCVFD, &cfg.nn_fd, &optlen);
  if (ec < 0) {
    fprintf(stderr,"nn_getsockopt: %s\n", nn_strerror(errno));
    goto done;
  }

  /* open the shr ring */
  cfg.ring = shr_open(cfg.file, SHR_WRONLY|SHR_BUFFERED);
  if (cfg.ring == NULL) goto done;

  /* add descriptors of interest to epoll */
  if (add_epoll(EPOLLIN, cfg.signal_fd)) goto done;
  if (add_epoll(EPOLLIN, cfg.nn_fd)) goto done;

  alarm(1);

  while (1) {
    ec = epoll_wait(cfg.epoll_fd, &ev, 1, -1);
    if (ec < 0) { 
      fprintf(stderr, "epoll: %s\n", strerror(errno));
      goto done;
    }

    if (ec == 0)                          { assert(0); goto done; }
    else if (ev.data.fd == cfg.signal_fd) { if (handle_signal() < 0) goto done; }
    else if (ev.data.fd == cfg.nn_fd)     { if (handle_nnmsg() < 0)  goto done; }
    else                                  { assert(0); goto done; }
  }
  
  rc = 0;
 
 done:
  if (cfg.ring) shr_close(cfg.ring);
  if (cfg.signal_fd != -1) close(cfg.signal_fd);
  if (cfg.epoll_fd != -1) close(cfg.epoll_fd);
  if (cfg.sock != -1) nn_close(cfg.sock);
  return 0;
}
