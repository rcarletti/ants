#ifndef PTASK_H_
#define PTASK_H_

#include <pthread.h>
#include <time.h>

struct task_par 
{
	int				arg;		/* task argument    */
	long			wcet;		/* in microseconds  */
	int 			period;		/* in milliseconds  */
	int				deadline;	/* relative (ms)    */
	int				priority;	/* in [0,99]        */
	int				dmiss;		/* no. of misses    */
	struct timespec at;			/* next activ. time */
	struct timespec dl;			/* abs. deadline    */
};

void set_period(struct task_par*);
void wait_for_period(struct task_par*);
int deadline_miss(struct task_par*);

void time_add_ms(struct timespec*, int);
int time_cmp(struct timespec, struct timespec);

pthread_t task_create(void *(*f)(void *), struct task_par * tp);


#endif /* PTASK_H_ */

