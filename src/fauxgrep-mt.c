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
  const char *line;
  const char *needle;
  int lineno;
  const char *path;
};

void grepline(const char *line, const char *needle, int lineno, const char *path)
{
  if (strstr(line, needle) != NULL)
  {
    printf("%s:%d: %s", path, lineno, line);
  }
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
      grepline(job->line, job->needle, job->lineno, job->path);
      free((void *)job->line);
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

  int num_threads = 1;
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
  char *path = NULL;
  for (int i = 0; i < num_threads; i++)
  {
    if (pthread_create(&threads[i], NULL, &worker, &jq) != 0)
    {
      err(1, "pthread_create() failed");
    }
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
      path = p->fts_path;
      FILE *f = fopen(path, "r");
      assert(f);
      int lineno = 1;
      while ((line_len = getline(&line, &buf_len, f)) != -1)
      {
        struct package *pkg = malloc(sizeof(struct package));
        pkg->line = strdup(line); // Don't forget to copy the line!
        pkg->lineno = lineno;
        pkg->needle = needle;
        pkg->path = path;
        job_queue_push(&jq, (void *)pkg);
        lineno++;
      }
      free(line);
      fclose(f);
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
