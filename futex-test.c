#include <errno.h>
#include <linux/futex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NUM_FUTEX 10
#define FUTEX_WAIT_MULTIPLE 31

struct futex_wait_block {
__u32 *uaddr;
__u32 val;
__u32 bitset;
};

#ifndef SYS_futex_waitv

// Assume x86_64
#define SYS_futex_waitv 449

// This struct is defined in newer (post kernel 5.16) headers
struct futex_waitv {
__u64 val;
__u64 uaddr;
__u32 flags;
__u32 __reserved;
};

#endif // SYS_futex_waitv

bool checkKernelType() {
    struct utsname name;

    if (uname(&name) == -1) {
        perror("uname");
        printf("uname call failed. Unable to determine kernel type. "
            "Futex syscall may cause undefined behavior on non-Linux kernels. Continue anyway? (Y/N) ");
    } else if (strcmp(name.sysname, "Linux") == 0) {
        printf("Kernel name: %s\nKernel version %s\nLinux kernel detected\n", name.sysname, name.release);
        return true;
    } else {
        printf("Kernel name: %s\nKernel version: %s\nNon-Linux kernel detected. "
            "Futex syscall may cause undefined behavior. Continue anyway? (Y/N) ", name.sysname, name.release);
    }
    while (1) {
        char answer = getchar();
        if (answer == 'Y' || answer == 'y') {
            return true;
        }
        if (answer == 'N' || answer == 'n') {
            return false;
        }
    }
}

/* Acquire the futex pointed to by 'futexp': wait for its value to
    become 1, and then set the value to 0. */
bool fwait(struct futex_wait_block futexp[]) {
    /* __sync_bool_compare_and_swap(ptr, oldval, newval) is a gcc
        built-in function.  It atomically performs the equivalent of:

            if (*ptr == oldval)
                *ptr = newval;

        It returns true if the test yielded true and *ptr was updated.
        The alternative here would be to employ the equivalent atomic
        machine-language instructions.  For further information, see
        the GCC Manual. */

    while (1) {

        /* Is the futex available? */

        bool isAvailable = false;
        for (int i = 0; i < NUM_FUTEX; i++) {
            if (__sync_bool_compare_and_swap(futexp[i].uaddr, 1, 0)) {
                isAvailable = true;
            }
        }

        if (isAvailable) {
            return true;
        }

        /* Futex is not available; wait */

        if (syscall(SYS_futex, futexp, FUTEX_WAIT_MULTIPLE, NUM_FUTEX, NULL, NULL, 0) == -1 && errno != EAGAIN) {
            perror("futex-FUTEX_WAIT_MULTIPLE");
            return false;
        }
    }
}

/* Release the futex pointed to by 'futexp': if the futex currently
    has the value 0, set its value to 1 and the wake any futex waiters,
    so that if the peer is blocked in fpost(), it can proceed. */

bool fpost(struct futex_wait_block futexp[], int i) {
    /* __sync_bool_compare_and_swap() was described in comments above */

    if (__sync_bool_compare_and_swap(futexp[i].uaddr, 0, 1)) {
        if (syscall(SYS_futex, futexp[i].uaddr, FUTEX_WAKE, 1, NULL, NULL, 0)  == -1) {
            perror("futex-FUTEX_WAKE");
            return false;
        }
    }
    return true;
}

/*
    This code was adapted from the example in the futex man page.
    It has been modified to test FUTEX_WAIT_MULTIPLE.
    We create 2 blocks of 10 futexes, fork the process, then choose one at random to wake on.

    nloops - number of times to loop (5 by default)
    verbose - print results of each loop to console (false by default)
    Above arguments can be specified at runtime ex. "./futex-test --verbose 50"

    Comment from man page:
    Demonstrate the use of futexes in a program where parent and child
    use a pair of futexes located inside a shared anonymous mapping to
    synchronize access to a shared resource: the terminal. The two
    processes each write 'num-loops' messages to the terminal and employ
    a synchronization protocol that ensures that they alternate in
    writing messages.
*/
bool testWaitMultiple(int nloops, bool verbose) {
    /* Create a shared anonymous mapping that will hold the futexes.
        Since the futexes are being shared between processes, we
        subsequently use the "shared" futex operations (i.e., not the
        ones suffixed "_PRIVATE") */

    __u32 *iaddr = mmap(NULL, sizeof(__u32) * 2 * NUM_FUTEX, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (iaddr == MAP_FAILED) {
        perror("mmap");
        return false;
    }

    struct futex_wait_block futex_block1[NUM_FUTEX], futex_block2[NUM_FUTEX];

    for (int i = 0; i < NUM_FUTEX; i++) {
        futex_block1[i].uaddr = &iaddr[i];
        *futex_block1[i].uaddr = 0;      /* State: unavailable */
        futex_block1[i].val = 0;
        futex_block1[i].bitset = FUTEX_BITSET_MATCH_ANY;
    }

    for (int i = 0; i < NUM_FUTEX; i++) {
        futex_block2[i].uaddr = &iaddr[NUM_FUTEX + i];
        *futex_block2[i].uaddr = 1;     /* State: available */
        futex_block2[i].val = 0;
        futex_block2[i].bitset = FUTEX_BITSET_MATCH_ANY;
    }

    /* Create a child process that inherits the shared anonymous
        mapping */

    pid_t childPid = fork();
    if (childPid == -1) {
        perror("fork");
        if (munmap(iaddr, sizeof(__u32) * 2 * NUM_FUTEX) == -1) {
            perror("munmap");
        }
        return false;
    }

    if (childPid == 0) {        /* Child */
        srand(time(NULL));
        int status = 0;
        for (int i = 0; i < nloops; i++) {
            if(!fwait(futex_block1)) {
                exit(EXIT_FAILURE);
            }
            if (verbose) {
                printf("Child  (%ld) %d\n", (long) getpid(), i);
            }
            if(!fpost(futex_block2, rand() % NUM_FUTEX)) {
                exit(EXIT_FAILURE);
            }
        }

        exit(EXIT_SUCCESS);
    }

    /* Parent falls through to here */

    srand(time(NULL) + 234251);
    bool success = true;
    for (int i = 0; i < nloops; i++) {
        if(!fwait(futex_block2)) {
            success = false;
            break;
        }
        if (verbose) {
            printf("Parent (%ld) %d\n", (long) getpid(), i);
        }
        if(!fpost(futex_block1, rand() % NUM_FUTEX)) {
            success = false;
            break;
        }
    }

    int childStatus;
    wait(&childStatus);
    if (childStatus == EXIT_FAILURE) {
        success = false;
    }
    if (munmap(iaddr, sizeof(__u32) * 2 * NUM_FUTEX) == -1) {
        perror("munmap");
    }
    return success;
}

/* futex2 has a sysfs interface, unlike the orignal futex, so we just check if that exists.
   NOTE: The syscall numbers for futex2 are likely to change between major kernel releases.
   If we wish to make an actual futex2 syscall in the future,
   we must read the files in sysfs to get the correct syscall number for the running kernel. */
bool testFutex2() {
    if (access("/sys/kernel/futex2", F_OK) == 0) {
        return true;
    } else {
        perror("access /sys/kernel/futex2");
        return false;
    }
}

/* Acquire the futex pointed to by 'futexp': wait for its value to
    become 1, and then set the value to 0. */
bool fwaitMainline(struct futex_waitv futexp[]) {
    /* __sync_bool_compare_and_swap(ptr, oldval, newval) is a gcc
        built-in function.  It atomically performs the equivalent of:

            if (*ptr == oldval)
                *ptr = newval;

        It returns true if the test yielded true and *ptr was updated.
        The alternative here would be to employ the equivalent atomic
        machine-language instructions.  For further information, see
        the GCC Manual. */

    while (1) {

        /* Is the futex available? */

        bool isAvailable = false;
        for (int i = 0; i < NUM_FUTEX; i++) {
            if (__sync_bool_compare_and_swap((__u32 *)futexp[i].uaddr, 1, 0)) {
                isAvailable = true;
            }
        }

        if (isAvailable) {
            return true;
        }

        /* Futex is not available; wait */

        if (syscall(SYS_futex_waitv, futexp, NUM_FUTEX, 0, NULL, 0) == -1 && errno != EAGAIN) {
            perror("futex-FUTEX_WAIT_MULTIPLE");
            return false;
        }
    }
}

/* Release the futex pointed to by 'futexp': if the futex currently
    has the value 0, set its value to 1 and the wake any futex waiters,
    so that if the peer is blocked in fpost(), it can proceed. */

bool fpostMainline(struct futex_waitv futexp[], int i) {
    /* __sync_bool_compare_and_swap() was described in comments above */

    if (__sync_bool_compare_and_swap((__u32 *)futexp[i].uaddr, 0, 1)) {
        if (syscall(SYS_futex, futexp[i].uaddr, FUTEX_WAKE, 1, NULL, NULL, 0)  == -1) {
            perror("futex-FUTEX_WAKE");
            return false;
        }
    }
    return true;
}

/* Futex 2 has been accepted in the mainline kernel as of 5.16.
   However, the interface is different from the previous implementations.
   There is no sysfs interface so we must make a syscall. */
bool testMainlineWaitv(int nloops, bool verbose) {
    /* Create a shared anonymous mapping that will hold the futexes.
        Since the futexes are being shared between processes, we
        subsequently use the "shared" futex operations (i.e., not the
        ones suffixed "_PRIVATE") */

    __u32 *iaddr = mmap(NULL, sizeof(__u32) * 2 * NUM_FUTEX, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (iaddr == MAP_FAILED) {
        perror("mmap");
        return false;
    }

    struct futex_waitv futex_block1[NUM_FUTEX], futex_block2[NUM_FUTEX];

    for (int i = 0; i < NUM_FUTEX; i++) {
        iaddr[i] = 0;    /* State: unavailable */
        futex_block1[i].uaddr = (uintptr_t)&iaddr[i];
        futex_block1[i].val = 0;
        futex_block1[i].flags = 2;    /* FUTEX_32 */
        futex_block1[i].__reserved = 0;
    }

    for (int i = 0; i < NUM_FUTEX; i++) {
        iaddr[NUM_FUTEX + i] = 1;    /* State: available */
        futex_block2[i].uaddr = (uintptr_t)&iaddr[NUM_FUTEX + i];
        futex_block2[i].val = 0;
        futex_block2[i].flags = 2;    /* FUTEX_32 */
        futex_block2[i].__reserved = 0;
    }

    /* Create a child process that inherits the shared anonymous
        mapping */

    pid_t childPid = fork();
    if (childPid == -1) {
        perror("fork");
        if (munmap(iaddr, sizeof(__u32) * 2 * NUM_FUTEX) == -1) {
            perror("munmap");
        }
        return false;
    }

    if (childPid == 0) {        /* Child */
        srand(time(NULL));
        int status = 0;
        for (int i = 0; i < nloops; i++) {
            if(!fwaitMainline(futex_block1)) {
                exit(EXIT_FAILURE);
            }
            if (verbose) {
                printf("Child  (%ld) %d\n", (long) getpid(), i);
            }
            if(!fpostMainline(futex_block2, rand() % NUM_FUTEX)) {
                exit(EXIT_FAILURE);
            }
        }

        exit(EXIT_SUCCESS);
    }

    /* Parent falls through to here */

    srand(time(NULL) + 234251);
    bool success = true;
    for (int i = 0; i < nloops; i++) {
        if(!fwaitMainline(futex_block2)) {
            success = false;
            break;
        }
        if (verbose) {
            printf("Parent (%ld) %d\n", (long) getpid(), i);
        }
        if(!fpostMainline(futex_block1, rand() % NUM_FUTEX)) {
            success = false;
            break;
        }
    }

    int childStatus;
    wait(&childStatus);
    if (childStatus == EXIT_FAILURE) {
        success = false;
    }
    if (munmap(iaddr, sizeof(__u32) * 2 * NUM_FUTEX) == -1) {
        perror("munmap");
    }
    return success;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    if (checkKernelType()) {
        int nloops = 5;
        bool verbose = false;
        if (argc > 1) {
            if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0) {
                verbose = true;
                if (argc > 2) {
                    nloops = atoi(argv[2]);
                }
            } else {
                nloops = atoi(argv[1]);
                if (argc > 2 && (strcmp(argv[2], "-v") == 0 || strcmp(argv[2], "--verbose") == 0)) {
                    verbose = true;
                }
            }
        }

        if (testWaitMultiple(nloops, verbose)) {
            puts("FUTEX_WAIT_MULTIPLE test successful");
        } else {
            puts("FUTEX_WAIT_MULTIPLE test failed");
        }
        if (testFutex2()) {
            puts("futex2 test successful");
        } else {
            puts("futex2 test failed");
        }
        if (testMainlineWaitv(nloops, verbose)) {
            puts("Mainline (kernel 5.16+) futex2 test successful");
        } else {
            puts("Mainline (kernel 5.16+) futex2 test failed");
        }
    }

    exit(EXIT_SUCCESS);
}
