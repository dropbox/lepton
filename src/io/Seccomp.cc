#include <stdio.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <linux/seccomp.h>

#include <sys/prctl.h>
#include <sys/syscall.h>
#include "seccomp-bpf.hh"
#include <linux/audit.h>

#if defined(__i386__)
#  define ARCH_NR AUDIT_ARCH_I386
#elif defined(__x86_64__)
#  define ARCH_NR AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#  define ARCH_NR AUDIT_ARCH_AARCH64
#elif defined(__arm__)
/*
 * <linux/audit.h> includes <linux/elf-em.h>, which does not define EM_ARM.
 * <linux/elf.h> only includes <asm/elf.h> if we're in the kernel.
 */
#  ifndef EM_ARM
#    define EM_ARM 40
#  endif
#  define ARCH_NR AUDIT_ARCH_ARM
#elif defined(__hppa__)
#  define ARCH_NR AUDIT_ARCH_PARISC
#elif defined(__ia64__)
#  define ARCH_NR AUDIT_ARCH_IA64
#elif defined(__mips__)
#  if defined(__mips64)
#    if defined(__MIPSEB__)
#      define ARCH_NR AUDIT_ARCH_MIPS64
#    else
#      define ARCH_NR AUDIT_ARCH_MIPSEL64
#    endif
#  else
#    if defined(__MIPSEB__)
#      define ARCH_NR AUDIT_ARCH_MIPS
#    else
#      define ARCH_NR AUDIT_ARCH_MIPSEL
#    endif
#  endif
#elif defined(__powerpc64__)
#  define ARCH_NR AUDIT_ARCH_PPC64
#elif defined(__powerpc__)
#  define ARCH_NR AUDIT_ARCH_PPC
#elif defined(__s390x__)
#  define ARCH_NR AUDIT_ARCH_S390X
#elif defined(__s390__)
#  define ARCH_NR AUDIT_ARCH_S390
#elif defined(__sparc__)
#  if defined(__arch64__)
#    define AUDIT_ARCH_SPARC64
#  else
#    define AUDIT_ARCH_SPARC
#  endif
#else
#  error "AUDIT_ARCH value unavailable"
#endif


#endif

namespace Sirikata {
bool installStrictSyscallFilter(bool verbose) {
#ifdef __linux__
    get_nprocs();
    get_nprocs_conf();
    struct sock_filter filter[] = {
        /* Validate architecture. */
        VALIDATE_ARCHITECTURE,
        /* Grab the system call number. */
        EXAMINE_SYSCALL,
        /* List allowed syscalls. */
        ALLOW_SYSCALL(rt_sigreturn),
#ifdef __NR_sigreturn
        ALLOW_SYSCALL(sigreturn),
#endif
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
        ALLOW_SYSCALL(madvise),
        ALLOW_SYSCALL(mmap),
        ALLOW_SYSCALL(brk),
        ALLOW_SYSCALL(munmap),
        ALLOW_SYSCALL(mprotect),
        ALLOW_SYSCALL(mremap),
        ALLOW_SYSCALL(futex),

#ifdef __i386__
       ALLOW_SYSCALL(mmap2),
#endif
#endif
        ALLOW_SYSCALL(exit),
        ALLOW_SYSCALL(read),
        ALLOW_SYSCALL(write),
        KILL_PROCESS,
    };
    struct sock_fprog prog;
    prog.len = (unsigned short)(sizeof(filter)/sizeof(filter[0]));
    prog.filter = filter;
    if (
#ifdef USE_STANDARD_MEMORY_ALLOCATORS
        true
#else
        prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT)
#endif
        ) {
#ifndef USE_STANDARD_MEMORY_ALLOCATORS
        if (verbose) {
            perror("prctl(SECCOMP)");
        }
#endif
        if (errno == EINVAL && verbose) {
            fprintf(stderr, "SECCOMP_MODE_STRICT is not available.\n%s",
                "Trying to set a filter to emulate strict mode\n");
        }
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
            if (verbose) {
                perror("prctl(NO_NEW_PRIVS)");
            }
            exit(1);
        }
        if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
            if (verbose) {
                perror("prctl(SECCOMP)");
            }
            exit(1);
        }
    }
    return true;
#else
    if (verbose) {
        fprintf(stderr, "SECCOMP not supported on this OS (linux only)\n");
    }
    return false;
#endif
}
}
