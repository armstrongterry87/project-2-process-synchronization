# Project 2: Process and Thread Synchronization

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

## Part 2: Ben's Chili Bowl (Monitors and Condition Variables)

[`part2/`](part2) simulates customers and cooks at Ben's Chili Bowl as pthreads that share one restaurant.

- The restaurant is a **monitor**. One `pthread_mutex_t` guards every field: the order queue, its size, order numbers, and the number of orders handled.
- Orders form a FIFO linked list (`AddOrderToBack` adds to the rear, and `GetOrder` takes from the front). The queue holds at most `max_size` orders.
- **`AddOrder`** (customers) waits on `can_add_orders` while the queue is full. It then gives the order its number, appends it, and signals `can_get_orders`.
- **`GetOrder`** (cooks) waits on `can_get_orders` while the queue is empty and orders are still expected. It then takes the front order and signals `can_add_orders`. When the last expected order is taken, it broadcasts `can_get_orders`, so every waiting cook wakes up, gets `NULL` ("No more orders"), and stops.
- Each **customer** thread places `ORDERS_PER_CUSTOMER` orders with random menu items. Each **cook** thread fulfills and frees orders until there are none left, then reports how many it fulfilled.
- **`CloseRestaurant`** checks that every expected order was fulfilled and the queue is empty. It then destroys the mutex and condition variables and frees the restaurant.

Tested with queue sizes as small as 1, more cooks than orders, and more customers than queue slots (1,000 runs with no deadlocks), plus ThreadSanitizer (no data races) and valgrind (no leaks).

### Build and run

```sh
cd part2
make
./main
```

The constants at the top of `main.c` (`BENSCHILLIBOWL_SIZE`, `NUM_CUSTOMERS`, `NUM_COOKS`, `ORDERS_PER_CUSTOMER`) can be changed to try other workloads.
