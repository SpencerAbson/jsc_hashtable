/* Non-threadsafe hash seed generation */

#include <stdint.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

uint32_t hashtable_seed = 0;

/* seed generation from processID and time (from github) */
static int pid_seed_generate(uint32_t *seed) {
#ifdef HAVE_GETTIMEOFDAY
    /* XOR of seconds and microseconds */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *seed = (uint32_t)tv.tv_sec ^ (uint32_t)tv.tv_usec;
#else
    /* Seconds only */
    *seed = (uint32_t)time(NULL);
#endif

    /* XOR with PID for more randomness */
#if defined(_WIN32)
    *seed ^= (uint32_t)GetCurrentProcessId();
#elif defined(HAVE_GETPID)
    *seed ^= (uint32_t)getpid();
#endif

    return 0;
}


void set_hashtable_seed(uint32_t new_seed)
{
    if (hashtable_seed == 0)
    {
        if (new_seed == 0)   // if no alternative seed given
            pid_seed_generate(&hashtable_seed);
    }
}
