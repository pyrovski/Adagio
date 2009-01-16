#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

extern int tdebug;
extern int etrace;
extern int errno;

static FILE *fp = NULL;

void tracefile_open(const char *filename) {
   char *s = strdup(filename);
   char *f = NULL;
   int l;
   int n;

   if (s == NULL)
      return;
   if ((f = strstr(s, "%p")) != NULL) {
      l = strlen(s) + 10;
      *f = '\0';
      f = malloc(sizeof(char) * l);
      n = snprintf(f, l, "%s%d%s", s, getpid(), s + strlen(s) + 2);
      f[n] = '\0';
      free(s);
      s = f;
   }

   if (etrace && (fp = fopen(s, "a")) == NULL) {
      fprintf(stderr, "Unable to open file '%s': %s\n", f, strerror(errno));
      exit(1);
   }
   free(s);
}

void tracefile_close() {
   if (fp != NULL)
      fclose(fp);
}

void add_time(unsigned int *tm, struct timeval *t) {
   struct timeval tv;

   if (tdebug) {
      gettimeofday(&tv, NULL);
      *tm += (tv.tv_usec - t->tv_usec);
      memcpy(t, &tv, sizeof(struct timeval));
   }
}

void trace_msg(const char *fmt, ...) {
   char *m = NULL;
   char *p = NULL;
   va_list ap;
   int n, size = 100;
   char *tfmt = NULL;
   time_t t = time(NULL);
   char *c = NULL;

   if ((p = malloc(size)) == NULL)
      return;
   while (1) {
      va_start(ap, fmt);
      n = vsnprintf(p, size, fmt, ap);
      va_end(ap);
      if (n > -1 && n < size)
         break;
      if (n > -1)
         size = n + 1;
      else
         size *= 2;
      if ((p = realloc(p, size)) == NULL)
         return;
   }
   if (tfmt != NULL) {
       m = (char *)calloc(40, sizeof(char));
       if (m == NULL)
          return;
       strftime(m, 40, tfmt, localtime(&t));
   } else {
      m = ctime(&t);
      c = strchr(m, '\n');
      if (c != NULL)
         *c = '\0';
   }
   if (fp != NULL)
      fprintf(fp, "%s %d %s\n", m, getpid(), p);
   else
      fprintf(stderr, "%s %d %s\n", m, getpid(), p);
   free(m);
   free(p);
}

