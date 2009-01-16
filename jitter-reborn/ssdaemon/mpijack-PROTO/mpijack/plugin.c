#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "plugin.h"
#include "impie.h"

static pid_t pid = 0;

void impie_debug(int what) {
   switch (what) {
      case IMPIE_DEBUG_FUNCTION:
         if (pid == 0)
            pid = getpid();
         printf("IMPIE[%d]: <time=%u, in=%s>\n",
            pid, (unsigned int)time(NULL), impie_function());
         break;
      case IMPIE_DEBUG_STATISTICS:
         impie_statistics();
         break;
      default:
         fprintf(stderr, "Invalid debug option '%d'\n", what);
         break;
   }
}

