#ifndef SR_COMMON_H
#define SR_COMMON_H

#define STATIC_VARS 0
#define DEBUG 1

#if STATIC_VARS
#define STATIC static
#else
#define STATIC
#endif

#define IOCSH_ARG static const iocshArg
#define IOCSH_ARG_ARRAY static const iocshArg *const
#define IOCSH_FUNCDEF static const iocshFuncDef

extern int save_restoreNFSOK;
extern int save_restoreIoErrors;

extern void makeNfsPath(char *dest, const char *s1, const char *s2);

/* strncpy sucks (may copy extra characters, may not null-terminate) */
#define strNcpy(dest, src, N)                                    \
    {                                                            \
        int ii;                                                  \
        char *dd = dest;                                         \
        const char *ss = src;                                    \
        if (dd && ss)                                            \
            for (ii = 0; *ss && ii < N - 1; ii++) *dd++ = *ss++; \
        *dd = '\0';                                              \
    }

#endif
