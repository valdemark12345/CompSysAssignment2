// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
  const char *needle;
  const char *path;
};
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

int fauxgrep_file(char const *needle, char const *path)
{
  FILE *f = fopen(path, "r");

  if (f == NULL)
  {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int lineno = 1;

  while (getline(&line, &linelen, f) != -1)
  {
    if (strstr(line, needle) != NULL)
    {
      pthread_mutex_lock(&stdout_mutex);
      printf("%s:%d: %s", path, lineno, line);
      pthread_mutex_unlock(&stdout_mutex);
    }

    lineno++;
  }

  free(line);
  fclose(f);

  return 0;
}

void *worker(void *arg)
{
  // job queue is argument
  struct job_queue *jq = arg;
  while (1)
  {
    struct package *job;
    // Take a package from the queue
    if (job_queue_pop(jq, (void **)&job) == 0)
    {
      // grep line it
      fauxgrep_file(job->needle, job->path);
      free(job);
    }
    else
    {
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

  int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
  char const *needle = argv[1];
  char *const *paths = &argv[2];
  if (argc > 3 && strcmp(argv[1], "-n") == 0)
  {
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
  // Initialize threads.
  for (int i = 0; i < num_threads; i++)
  {
    if (pthread_create(&threads[i], NULL, &worker, &jq) != 0)
    {
      err(1, "pthread_create() failed");
    }
  }

  FTSENT *p;
  struct package *pkg;
  while ((p = fts_read(ftsp)) != NULL)
  {
    switch (p->fts_info)
    {
    case FTS_D:
      break;
    case FTS_F:
      pkg = malloc(sizeof(struct package));
      pkg->needle = needle;
      pkg->path = strdup(p->fts_path);
      job_queue_push(&jq, (void *)pkg);
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

  return 0;
}
