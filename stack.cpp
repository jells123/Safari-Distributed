#include "stack.h"

pthread_mutex_t stack_mtx;
struct stack *stack;

void push_pkt( packet *pakiet, MPI_Status status )
{
    struct stack *tmp = (struct stack*) malloc(sizeof(struct stack));
    tmp->pakiet = pakiet;
    tmp->status = status;

    pthread_mutex_lock(&stack_mtx);
    tmp->prev = stack;
    stack = tmp;
    pthread_mutex_unlock(&stack_mtx);
}

packet *pop_pkt( MPI_Status *status )
{
    pthread_mutex_lock(&stack_mtx);
    if (stack == NULL ) {
	pthread_mutex_unlock(&stack_mtx);
        return NULL;
    } else {
        packet *tmp = stack->pakiet;
        struct stack *prev = stack->prev;
        *status= stack->status;
        free(stack);
        stack = prev;
	pthread_mutex_unlock(&stack_mtx);
        return tmp;
    }
}