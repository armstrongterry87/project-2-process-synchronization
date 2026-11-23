/*
 * Project 2 (Part 1): Process Synchronization Using Semaphores
 * Poor Student / Dear Old Dad (+ Lovable Mom and N students, extra credit)
 *
 * Usage:  shm_proc <parents> <children>
 *           parents  = 1 (Dear Old Dad) or 2 (Dear Old Dad + Lovable Mom)
 *           children = N > 0 Poor Students
 *         shm_proc          (no arguments) runs the original problem: 1 1
 *
 * BankAccount is a single int in System V shared memory. Every read-modify-
 * write of BankAccount happens between sem_wait(mutex) and sem_post(mutex),
 * so no two processes can update the account at the same time (no race
 * conditions). Each process keeps its own non-shared copy, localBalance.
 *
 * The program loops forever. To stop it, send a signal to the parent
 * (Dear Old Dad), e.g. `kill <pid>` or Ctrl-C. The parent then stops its
 * children, waits for them, and removes the shared memory.
 */
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <unistd.h>
#include  <sys/wait.h>
#include  <semaphore.h>
#include  <fcntl.h>
#include  <signal.h>
#include  <time.h>
#include  <errno.h>
#include  <sys/prctl.h>

#define SEM_NAME     "/psddsemaphore"
#define MAX_CHILDREN 1000

static sem_t *mutex;                       /* guards BankAccount            */
static volatile sig_atomic_t running = 1;  /* cleared by SIGINT / SIGTERM   */

static void stopHandler(int sig) {
    (void)sig;
    running = 0;
}

/* Seed each process differently so they don't all make the same choices. */
static void seedRandom(void) {
    srand((unsigned)time(NULL) ^ ((unsigned)getpid() << 16));
}

/* --------------------------------------------------------------------- */
/* Dear Old Dad                                                            */
/* --------------------------------------------------------------------- */
static void DadProcess(int *BankAccount) {
    int localBalance = 0;

    while (running) {
        /* 1. Sleep some random amount of time between 0 - 5 seconds */
        sleep(rand() % 6);
        if (!running) break;

        /* 2. */
        printf("Dear Old Dad: Attempting to Check Balance\n");

        /* 3. Even: deposit if the last balance is < 100 */
        if (rand() % 2 == 0) {
            if (localBalance < 100) {
                /* Deposit Money (critical section) */
                sem_wait(mutex);
                localBalance = *BankAccount;               /* 1. copy in   */
                int amount = rand() % 101;                 /* 2. 0 - 100   */
                if (amount % 2 == 0) {                     /* 3. even      */
                    localBalance += amount;
                    printf("Dear Old Dad: Deposits $%d / Balance = $%d\n",
                           amount, localBalance);
                    *BankAccount = localBalance;           /* 4. copy out  */
                } else {                                   /* 5. odd       */
                    printf("Dear Old Dad: Doesn't have any money to give\n");
                }
                sem_post(mutex);
            } else {
                printf("Dear Old Dad: Thinks Student has enough Cash ($%d)\n",
                       localBalance);
            }
        } else {
            /* 4. Odd: just check the balance */
            sem_wait(mutex);
            localBalance = *BankAccount;
            sem_post(mutex);
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }
    }
}

/* --------------------------------------------------------------------- */
/* Lovable Mom (extra credit)                                              */
/* --------------------------------------------------------------------- */
static void MomProcess(int *BankAccount) {
    int localBalance = 0;

    while (running) {
        /* 1. Sleep some random amount of time between 0 - 10 seconds */
        sleep(rand() % 11);
        if (!running) break;

        /* 2. */
        printf("Loveable Mom: Attempting to Check Balance\n");

        /* 3. If the last balance is <= 100, always deposit */
        if (localBalance <= 100) {
            /* Deposit Money (critical section) */
            sem_wait(mutex);
            localBalance = *BankAccount;                   /* 1. copy in   */
            int amount = rand() % 126;                     /* 2. 0 - 125   */
            localBalance += amount;                        /* 3. deposit   */
            printf("Loveable Mom: Deposits $%d / Balance = $%d\n",
                   amount, localBalance);
            *BankAccount = localBalance;                   /* 4. copy out  */
            sem_post(mutex);
        } else {
            /* Balance is already high: refresh it so she checks again later */
            sem_wait(mutex);
            localBalance = *BankAccount;
            sem_post(mutex);
            printf("Loveable Mom: Last Checking Balance = $%d\n", localBalance);
        }
    }
}

/* --------------------------------------------------------------------- */
/* Poor Student #id                                                        */
/* --------------------------------------------------------------------- */
static void StudentProcess(int *BankAccount, int id) {
    int localBalance = 0;

    while (running) {
        /* 1. Sleep some random amount of time between 0 - 5 seconds */
        sleep(rand() % 6);
        if (!running) break;

        /* 2. */
        printf("Poor Student #%d: Attempting to Check Balance\n", id);

        if (rand() % 2 == 0) {
            /* 3. Even: Withdraw Money (critical section) */
            sem_wait(mutex);
            localBalance = *BankAccount;                   /* 1. copy in   */
            int need = rand() % 51;                        /* 2. 0 - 50    */
            printf("Poor Student needs $%d\n", need);
            if (need <= localBalance) {                    /* 3. withdraw  */
                localBalance -= need;
                printf("Poor Student #%d: Withdraws $%d / Balance = $%d\n",
                       id, need, localBalance);
            } else {                                       /* 4. too much  */
                printf("Poor Student #%d: Not Enough Cash ($%d)\n",
                       id, localBalance);
            }
            *BankAccount = localBalance;                   /* 5. copy out  */
            sem_post(mutex);
        } else {
            /* 4. Odd: just check the balance */
            sem_wait(mutex);
            localBalance = *BankAccount;
            sem_post(mutex);
            printf("Poor Student #%d: Last Checking Balance = $%d\n",
                   id, localBalance);
        }
    }
}

/* Child setup: default signal handling, die if Dad dies, fresh random seed. */
static void childSetup(void) {
    struct sigaction sa;
    sa.sa_handler = stopHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() == 1) exit(0);   /* parent already gone */
    seedRandom();
}

int  main(int  argc, char *argv[])
{
    int    ShmID;
    int    *ShmPTR;
    int    parents = 1, children = 1;
    pid_t  pids[MAX_CHILDREN + 1];
    int    nprocs = 0;

    /* Print each line as soon as it is written, even through a pipe. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc == 3) {
        parents = atoi(argv[1]);
        children = atoi(argv[2]);
    }
    if (argc == 2 || argc > 3 || (parents != 1 && parents != 2) ||
        children < 1 || children > MAX_CHILDREN) {
        printf("usage:  %s <parents> <children> (where parent = [1 | 2], "
               "children = [N > 0]\n", argv[0]);
        exit(1);
    }

    /* Shared memory holding one int: the BankAccount */
    ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) {
        printf("*** shmget error ***\n");
        exit(1);
    }
    ShmPTR = (int *) shmat(ShmID, NULL, 0);
    if (ShmPTR == (int *) -1) {
        printf("*** shmat error ***\n");
        shmctl(ShmID, IPC_RMID, NULL);
        exit(1);
    }
    *ShmPTR = 0;                      /* BankAccount starts at $0 */

    /* Semaphore (initial value 1) used as a mutex around BankAccount.
       Unlinking the name right away keeps it shared by our processes
       (they inherit it across fork) without leaving a stale name behind. */
    sem_unlink(SEM_NAME);
    if ((mutex = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1)) == SEM_FAILED) {
        perror("semaphore initilization");
        shmdt((void *) ShmPTR);
        shmctl(ShmID, IPC_RMID, NULL);
        exit(1);
    }
    sem_unlink(SEM_NAME);

    /* Dad (the parent) stops cleanly on SIGINT / SIGTERM */
    struct sigaction sa;
    sa.sa_handler = stopHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Lovable Mom: an additional parent process */
    if (parents == 2) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("*** fork error ***\n");
            running = 0;
        } else if (pid == 0) {
            childSetup();
            MomProcess(ShmPTR);
            shmdt((void *) ShmPTR);
            exit(0);
        } else {
            pids[nprocs++] = pid;
        }
    }

    /* N Poor Students */
    for (int i = 1; i <= children && running; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("*** fork error ***\n");
            running = 0;
            break;
        } else if (pid == 0) {
            childSetup();
            StudentProcess(ShmPTR, i);
            shmdt((void *) ShmPTR);
            exit(0);
        }
        pids[nprocs++] = pid;
    }

    /* Dear Old Dad runs in the original (parent) process */
    seedRandom();
    DadProcess(ShmPTR);

    /* Signal received: stop the children, wait for them, clean up */
    for (int i = 0; i < nprocs; i++)
        kill(pids[i], SIGTERM);
    for (int i = 0; i < nprocs; i++)
        while (waitpid(pids[i], NULL, 0) < 0 && errno == EINTR)
            ;

    sem_close(mutex);
    shmdt((void *) ShmPTR);
    shmctl(ShmID, IPC_RMID, NULL);
    printf("Dear Old Dad: All processes stopped, shared memory removed. Bye!\n");
    return 0;
}
