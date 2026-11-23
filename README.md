# Project 2: Process Synchronization

## Part 1: Poor Student / Dear Old Dad (Semaphores)

[`part1/shm_processes.c`](part1/shm_processes.c) solves the Poor Student / Dear Old Dad IPC problem with System V shared memory and a POSIX semaphore. It also includes the extra credit: a Lovable Mom process and N Poor Students.

- `BankAccount` is one `int` in shared memory (`shmget`/`shmat`) and starts at $0.
- A semaphore with initial value 1 (`sem_open`) works as a mutex. Every read-modify-write of `BankAccount` happens between `sem_wait` and `sem_post`, so deposits and withdrawals never race.
- Each process keeps its own non-shared `localBalance` and copies it to and from `BankAccount` inside the critical section.
- **Dear Old Dad** (the original process) sleeps 0–5 s. On an even roll, if his last balance is below $100, he tries to deposit $0–100; an even amount gets deposited, an odd one means he has no money to give. On an odd roll, he just checks the balance.
- **Lovable Mom** (extra credit) sleeps 0–10 s and always deposits $0–125 when the last balance is $100 or less.
- **Poor Student #1…N** sleep 0–5 s. On an even roll, a student needs $0–50 and withdraws it if the balance covers it; otherwise it's "Not Enough Cash". On an odd roll, the student just checks the balance.
- Sending SIGINT or SIGTERM to the parent (Ctrl-C or `kill <pid>`) stops every process cleanly. Dad terminates and reaps the children, then detaches and removes the shared memory.

### Build and run

```sh
cd part1
make
./shm_proc          # Dear Old Dad + 1 Poor Student (original problem)
./shm_proc 1 3      # Dear Old Dad + 3 Poor Students
./shm_proc 2 10     # Dear Old Dad + Lovable Mom + 10 Poor Students
```

Usage: `shm_proc <parents> <children>`, where parents = 1 or 2 and children = N > 0.
