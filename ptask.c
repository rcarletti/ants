#include "ptask.h"


static void time_copy(struct timespec *td, struct timespec ts)
{
	td->tv_sec  = ts.tv_sec;
	td->tv_nsec = ts.tv_nsec;
}

void time_add_ms(struct timespec *t, int ms)
{
	t->tv_sec += ms/1000;
	t->tv_nsec += (ms%1000)*1000000;

	while (t->tv_nsec > 1000000000)
	{
		t->tv_nsec -= 1000000000;
		t->tv_sec += 1;
	}
}

int time_cmp(struct timespec t1, struct timespec t2)
{
	if (t1.tv_sec > t2.tv_sec) return 1;
	if (t1.tv_sec < t2.tv_sec) return -1;
	if (t1.tv_nsec > t2.tv_nsec) return 1;
	if (t1.tv_nsec < t2.tv_nsec) return -1;
	return 0;
}

void set_period(struct task_par *tp)
{
struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	time_copy(&(tp->at), t);
	time_copy(&(tp->dl), t);
	time_add_ms(&(tp->at), tp->period);
	time_add_ms(&(tp->dl), tp->deadline);
}

void wait_for_period(struct task_par *tp)
{
	clock_nanosleep(CLOCK_MONOTONIC,
		TIMER_ABSTIME, &(tp->at), NULL);

	time_add_ms(&(tp->at), tp->period);
	time_add_ms(&(tp->dl), tp->period);
}

int deadline_miss(struct task_par *tp)
{
struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if (time_cmp(now, tp->dl) > 0) 
	{
		tp->dmiss++;
		printf("%ld\n", (tp->dl.tv_nsec - now.tv_nsec)/1000000);
		return 1;
	}
	return 0;
}

pthread_t task_create(void *(*f)(void *), struct task_par * tp)
{
pthread_attr_t		myatt;
struct sched_param	mypar;
pthread_t			tid;

	pthread_attr_init(&myatt);
	
	pthread_attr_setinheritsched(&myatt, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&myatt, SCHED_FIFO);

	mypar.sched_priority = tp->priority;
	pthread_attr_setschedparam(&myatt, &mypar);

	pthread_create(&tid, &myatt, f, tp);

	pthread_attr_destroy(&myatt);

	return tid;
}
