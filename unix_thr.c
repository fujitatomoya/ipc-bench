/*
    Measure throughput of IPC using unix domain sockets.


    Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) &&                           \
    defined(_POSIX_MONOTONIC_CLOCK)
#define HAS_CLOCK_GETTIME_MONOTONIC
#endif

int main(int argc, char *argv[]) {
  int fds[2]; /* the pair of socket descriptors */
  int dgram_enable = 0;
  int ret;
  int size;
  char *buf;
  int64_t count, i, delta;
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
  struct timespec start, stop;
#else
  struct timeval start, stop;
#endif

  if ((argc < 3) | (argc > 4)) {
    printf("usage: unix_thr <message-size> <roundtrip-count> <dgram-enable(default stream)>\n");
    return 1;
  }

  size = atoi(argv[1]);
  count = atol(argv[2]);

  if (argc == 4) {
    dgram_enable = atol(argv[3]);
  }

  buf = malloc(size);
  if (buf == NULL) {
    perror("malloc");
    return 1;
  }

  printf("message size: %i octets\n", size);
  printf("message count: %li\n", count);
  printf("socket_type: %s\n", (dgram_enable ? "SOCK_DGRAM" : "SOCK_STREAM"));

  if (dgram_enable == 1) {
    ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);
  } else {
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  }
  if (ret == -1) {
    perror("socketpair");
    return 1;
  }

  if (!fork()) {
    /* child */

    for (i = 0; i < count; i++) {
      if (read(fds[1], buf, size) != size) {
        perror("read");
        return 1;
      }
    }
  } else {
/* parent */

#ifdef HAS_CLOCK_GETTIME_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
      perror("clock_gettime");
      return 1;
    }
#else
    if (gettimeofday(&start, NULL) == -1) {
      perror("gettimeofday");
      return 1;
    }
#endif

    for (i = 0; i < count; i++) {
      if (write(fds[0], buf, size) != size) {
        perror("write");
        return 1;
      }
    }

#ifdef HAS_CLOCK_GETTIME_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1) {
      perror("clock_gettime");
      return 1;
    }

    delta = ((stop.tv_sec - start.tv_sec) * 1000000 +
             (stop.tv_nsec - start.tv_nsec) / 1000);

#else
    if (gettimeofday(&stop, NULL) == -1) {
      perror("gettimeofday");
      return 1;
    }

    delta =
        (stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_usec - start.tv_usec);

#endif

    printf("average throughput: %li msg/s\n", (count * 1000000) / delta);
    printf("average throughput: %li Mb/s\n",
           (((count * 1000000) / delta) * size * 8) / 1000000);
  }

  return 0;
}
