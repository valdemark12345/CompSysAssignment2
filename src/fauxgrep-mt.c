// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"

struct package
{
  struct job_queue *jq;
  const char *line;
  const char *needle;
  int *lineno;
  const char *path;
};

void grepline(const char *line, const char *needle, int *lineno, const char *path)
{
  if (strstr(line, needle) != NULL)
  {
    printf("%s:%d: %s", path, *lineno, line);
  }
}

void *worker(void *arg)
{
  struct package *package = arg;
  while (1)
  {
    char *line;
    if (job_queue_pop(package->jq, (void **)&line) == 0)
    {
      grepline(package->line, package->needle, package->lineno, package->path);
      pthread_mutex_lock(&package->jq->lock);
      package->lineno++;
      pthread_mutex_unlock(&package->jq->lock);
      free(line);
    }
    else
    {
      // If job_queue_pop() returned non-zero, that means the queue is
      // being killed (or some other error occured).  In any case,
      // that means it's time for this thread to die.
      break;
    }
  }

  return NULL;
}

int main(int argc, char *const *argv)
{
  if (argc < 2)
  {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle = argv[1];
  char *const *paths = &argv[2];
  int *lineno = 0;
  if (argc > 3 && strcmp(argv[1], "-n") == 0)
  {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1)
    {
      err(1, "invalid thread count: %s", argv[2]);
    }

    needle = argv[3];
    paths = &argv[4];
  }
  else
  {
    needle = argv[1];
    paths = &argv[2];
  }

  struct job_queue jq;
  job_queue_init(&jq, 64);

  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL)
  {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  ssize_t line_len;
  size_t buf_len = 0;
  char *line = NULL;
  while ((p = fts_read(ftsp)) != NULL)
  {
    switch (p->fts_info)
    {
    case FTS_D:
      break;
    case FTS_F:

      while ((line_len = getline(&line, &buf_len, stdin)) != -1)
      {
        job_queue_push(&jq, (void *)strdup(line));
      }
      free(line);
      for (int i = 0; i < num_threads; i++)
      {
        struct package *pac = malloc(sizeof(struct package));
        pac->jq = &jq;
        pac->lineno = lineno;
        pac->needle = needle;
        pac->path = p->fts_path;
        if (pthread_create(&threads[i], NULL, &worker, &pac) != 0)
        {
          err(1, "pthread_create() failed");
        }
      }
      assert(0); // Process the file p->fts_path, somehow.
      break;
    default:
      break;
    }
  }

  fts_close(ftsp);

  // Destroy the queue.
  job_queue_destroy(&jq);
  for (int i = 0; i < num_threads; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
    {
      err(1, "pthread_join() failed");
    }
  }

  assert(0); // Shut down the job queue and the worker threads here.

  return 0;
}
