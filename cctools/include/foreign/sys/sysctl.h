#ifndef _FOREIGN_SYS_SYSCTL_H
#define _FOREIGN_SYS_SYSCTL_H

/*
 * Prefer the system <sys/sysctl.h> where it exists (macOS, *BSD).
 *
 * glibc 2.32+ no longer ships <sys/sysctl.h> and Windows/Cygwin never did,
 * so fall back to a minimal stub that keeps the cctools sources compiling.
 * sysctl() itself will simply fail at runtime on those platforms.
 */
#ifndef __CYGWIN__
#  if defined(__has_include_next)
#    if __has_include_next(<sys/sysctl.h>)
#      include_next <sys/sysctl.h>
#    else
#      define CCTOOLS_SYSCTL_STUB 1
#    endif
#  else
#    include_next <sys/sysctl.h>
#  endif
#else
#  define CCTOOLS_SYSCTL_STUB 1
#endif

#ifdef CCTOOLS_SYSCTL_STUB
#ifndef __SYSCTL_H__
#define __SYSCTL_H__

#include <errno.h>
#include <stdio.h> /* stderr */
#include <sys/types.h>

#define CTL_KERN 1
#define KERN_OSRELEASE 2

static inline
int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp,
           const void *newp, size_t newlen)
{
    /* fprintf(stderr, "sysctl() not implented\n"); */
    (void)name; (void)namelen; (void)oldp; (void)oldlenp;
    (void)newp; (void)newlen;
    errno = EINVAL;
    return -1;
}
#endif /* __SYSCTL_H__ */
#endif /* CCTOOLS_SYSCTL_STUB */

#endif /* _FOREIGN_SYS_SYSCTL_H */
