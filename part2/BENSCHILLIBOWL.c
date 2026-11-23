#include "BENSCHILLIBOWL.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

MenuItem BENSCHILLIBOWLMenu[] = {
    "BensChilli",
    "BensHalfSmoke",
    "BensHotDog",
    "BensChilliCheeseFries",
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
int BENSCHILLIBOWLMenuLength = 10;

/*
 * The restaurant works as a monitor: bcb->mutex protects every field of the
 * restaurant, and two condition variables let threads wait for a change:
 *   - can_add_orders: signalled when an order is removed (queue not full)
 *   - can_get_orders: signalled when an order is added (queue not empty),
 *                     or broadcast when every expected order has been taken
 * Orders form a FIFO linked list: bcb->orders points at the front, new
 * orders go on the back.
 */

/* Select a random item from the Menu and return it */
MenuItem PickRandomMenuItem() {
    return BENSCHILLIBOWLMenu[rand() % BENSCHILLIBOWLMenuLength];
}

/* Allocate memory for the Restaurant, then create the mutex and condition variables needed to instantiate the Restaurant */

BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    BENSCHILLIBOWL *bcb = (BENSCHILLIBOWL *) malloc(sizeof(BENSCHILLIBOWL));
    if (bcb == NULL) {
        perror("malloc");
        exit(1);
    }

    bcb->orders = NULL;
    bcb->current_size = 0;
    bcb->max_size = max_size;
    bcb->next_order_number = 1;
    bcb->orders_handled = 0;
    bcb->expected_num_orders = expected_num_orders;

    pthread_mutex_init(&bcb->mutex, NULL);
    pthread_cond_init(&bcb->can_add_orders, NULL);
    pthread_cond_init(&bcb->can_get_orders, NULL);

    printf("Restaurant is open!\n");
    return bcb;
}


/* check that the number of orders received is equal to the number handled (ie.fullfilled). Remember to deallocate your resources */

void CloseRestaurant(BENSCHILLIBOWL* bcb) {
    pthread_mutex_lock(&bcb->mutex);

    /* Every order must have been fulfilled before closing. */
    if (bcb->orders_handled != bcb->expected_num_orders || !IsEmpty(bcb)) {
        printf("%d orders not fulfilled\n",
               bcb->expected_num_orders - bcb->orders_handled);
    }
    assert(IsEmpty(bcb));
    assert(bcb->orders_handled == bcb->expected_num_orders);

    /* Free anything still in the queue (nothing, if all went well). */
    while (bcb->orders != NULL) {
        Order *next = bcb->orders->next;
        free(bcb->orders);
        bcb->orders = next;
    }

    pthread_mutex_unlock(&bcb->mutex);

    pthread_mutex_destroy(&bcb->mutex);
    pthread_cond_destroy(&bcb->can_add_orders);
    pthread_cond_destroy(&bcb->can_get_orders);
    free(bcb);

    printf("Restaurant is closed!\n");
}

/* add an order to the back of queue */
int AddOrder(BENSCHILLIBOWL* bcb, Order* order) {
    pthread_mutex_lock(&bcb->mutex);

    /* Wait until the restaurant is not full. */
    while (IsFull(bcb)) {
        pthread_cond_wait(&bcb->can_add_orders, &bcb->mutex);
    }

    order->order_number = bcb->next_order_number++;
    order->next = NULL;
    AddOrderToBack(&bcb->orders, order);
    bcb->current_size++;
    int order_number = order->order_number;

    /* There is now at least one order for a cook to take. */
    pthread_cond_signal(&bcb->can_get_orders);
    pthread_mutex_unlock(&bcb->mutex);

    return order_number;
}

/* remove an order from the queue */
Order *GetOrder(BENSCHILLIBOWL* bcb) {
    pthread_mutex_lock(&bcb->mutex);

    /* Wait until there is an order, unless every expected order is taken. */
    while (IsEmpty(bcb) && bcb->orders_handled < bcb->expected_num_orders) {
        pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex);
    }

    if (IsEmpty(bcb)) {
        /* No orders left: wake the other cooks so they can stop too. */
        printf("No more orders\n");
        pthread_cond_broadcast(&bcb->can_get_orders);
        pthread_mutex_unlock(&bcb->mutex);
        return NULL;
    }

    /* Take the order at the front of the queue. */
    Order *order = bcb->orders;
    bcb->orders = order->next;
    order->next = NULL;
    bcb->current_size--;
    bcb->orders_handled++;

    /* There is now room for a customer to add an order. */
    pthread_cond_signal(&bcb->can_add_orders);

    /* That was the last expected order: tell the waiting cooks. */
    if (bcb->orders_handled == bcb->expected_num_orders) {
        pthread_cond_broadcast(&bcb->can_get_orders);
    }

    pthread_mutex_unlock(&bcb->mutex);
    return order;
}

// Optional helper functions (you can implement if you think they would be useful)
bool IsEmpty(BENSCHILLIBOWL* bcb) {
  return bcb->current_size == 0;
}

bool IsFull(BENSCHILLIBOWL* bcb) {
  return bcb->current_size >= bcb->max_size;
}

/* this methods adds order to rear of queue */
void AddOrderToBack(Order **orders, Order *order) {
    order->next = NULL;
    if (*orders == NULL) {
        *orders = order;
        return;
    }
    Order *curr = *orders;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = order;
}

