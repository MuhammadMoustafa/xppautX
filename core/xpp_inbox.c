/* The protocol's input queues (xpp_inbox.h).

   The core runs on the main thread and never reads a file descriptor or a
   socket for protocol input: reader threads push lines here and the core
   takes them with xpp_inbox_next(). That keeps input moving while the core
   computes, so a later classifier can pick out Abort or Quit at once.

   Two locks: push_lock serialises pushers (sequence number, classifier,
   enqueue happen as one step, so sequence order is queue order) and is
   never taken by the core; lock guards the queues and is held only briefly.
   Waiting is on a condition variable, never a polling loop. This file
   includes no core header. */
#include "xpp_inbox.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#ifdef _WIN32
#include "xpp_win32.h"
#else
#include <unistd.h>
#endif

typedef struct Item {
    struct Item *next;
    unsigned long seq;
    char *line;
} Item;

typedef struct {
    Item *head, *tail;
} Queue;

static pthread_mutex_t push_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static Queue queues[2]; /* [XPP_INBOX_NORMAL], [XPP_INBOX_CONTROL] */
static int closed;
static unsigned long next_seq = 1;
static int (*classify)(const char *line, unsigned long seq);

void xpp_inbox_set_classifier(int (*cls)(const char *line, unsigned long seq))
{
    pthread_mutex_lock(&push_lock);
    classify = cls;
    pthread_mutex_unlock(&push_lock);
}

void xpp_inbox_push(const char *line, size_t n)
{
    Item *it = malloc(sizeof *it);
    int q = XPP_INBOX_NORMAL;
    it->next = NULL;
    it->line = malloc(n + 1);
    memcpy(it->line, line, n);
    it->line[n] = 0;
    pthread_mutex_lock(&push_lock);
    it->seq = next_seq++;
    if (classify && classify(it->line, it->seq) == XPP_INBOX_CONTROL) q = XPP_INBOX_CONTROL;
    pthread_mutex_lock(&lock);
    if (queues[q].tail) queues[q].tail->next = it;
    else queues[q].head = it;
    queues[q].tail = it;
    /* broadcast: the core may wait on one queue while a line lands in the other */
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
    pthread_mutex_unlock(&push_lock);
}

void xpp_inbox_close(void)
{
    pthread_mutex_lock(&lock);
    closed = 1;
    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&lock);
}

/* the queue to take from now, or -1; call with the lock held */
static int pick(int which)
{
    if (which != XPP_INBOX_NORMAL && queues[XPP_INBOX_CONTROL].head) return XPP_INBOX_CONTROL;
    if (which != XPP_INBOX_CONTROL && queues[XPP_INBOX_NORMAL].head) return XPP_INBOX_NORMAL;
    return -1;
}

int xpp_inbox_next(int which, int wait_ms, char **line, unsigned long *seq)
{
    struct timespec until;
    int q, r = 0;
    if (wait_ms > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        until.tv_sec = now.tv_sec + wait_ms / 1000;
        until.tv_nsec = now.tv_usec * 1000L + (wait_ms % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) {
            until.tv_sec++;
            until.tv_nsec -= 1000000000L;
        }
    }
    pthread_mutex_lock(&lock);
    while ((q = pick(which)) < 0 && !closed && wait_ms != 0) {
        if (wait_ms < 0) pthread_cond_wait(&ready, &lock);
        else if (pthread_cond_timedwait(&ready, &lock, &until) == ETIMEDOUT) break;
    }
    if (q < 0) q = pick(which); /* a line may have come with the timeout */
    if (q >= 0) {
        Item *it = queues[q].head;
        if (!(queues[q].head = it->next)) queues[q].tail = NULL;
        *line = it->line;
        if (seq) *seq = it->seq;
        free(it);
        r = 1;
    } else if (closed && !queues[0].head && !queues[1].head) {
        r = -1;
    }
    pthread_mutex_unlock(&lock);
    return r;
}

/* ---- the --server reader: stdin, line by line ---------------------------- */

/* A line longer than this is dropped up to its newline (the old read_line
   gave up at 256 MB too); pixel answers are a few MB. */
#define MAX_LINE ((size_t)256 << 20)
#define CHUNK 65536

/* up to n bytes of stdin, blocking; <= 0 at end of input or on an error */
static long read_stdin(char *buf, size_t n)
{
#ifdef _WIN32
    return xpp_read_stdin(buf, (int)n);
#else
    for (;;) {
        ssize_t r = read(0, buf, n);
        if (r < 0 && errno == EINTR) continue;
        return (long)r;
    }
#endif
}

static void *stdin_main(void *arg)
{
    char *buf = NULL;
    size_t len = 0, cap = 0, scanned = 0;
    int skipping = 0; /* inside an overlong line, until its newline */
    (void)arg;
    for (;;) {
        char *start, *nl;
        long r;
        if (cap - len < CHUNK) {
            cap = cap * 2 + CHUNK;
            buf = realloc(buf, cap);
        }
        r = read_stdin(buf + len, cap - len);
        if (r <= 0) break;
        len += (size_t)r;
        /* push every complete line; search only the bytes not searched yet */
        start = buf;
        while ((nl = memchr(buf + scanned, '\n', len - scanned)) != NULL) {
            size_t n = (size_t)(nl - start);
            if (n > 0 && start[n - 1] == '\r') n--;
            if (!skipping) xpp_inbox_push(start, n);
            skipping = 0;
            start = nl + 1;
            scanned = (size_t)(start - buf);
        }
        if (start != buf) {
            len -= (size_t)(start - buf);
            memmove(buf, start, len);
        }
        scanned = len;
        if (len > MAX_LINE) {
            skipping = 1;
            len = scanned = 0;
        }
    }
    free(buf);
    xpp_inbox_close();
    return NULL;
}

int xpp_inbox_start_stdin(void)
{
    pthread_t t;
    if (pthread_create(&t, NULL, stdin_main, NULL) != 0) return 0;
    pthread_detach(t);
    return 1;
}
