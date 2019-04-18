#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define DATA_ALL 1000000
#define DATA_MAX 500000
#define QUEUE_MAX_NODE_NUM 2000000
#define WORKER_MAX_NUM 20
#define SORT_MIN_NUM 1000
#define random(x) (rand()%x)

struct queue_node
{
	int begin;
	int end;
};

struct queue_node queue_data[QUEUE_MAX_NODE_NUM];
//int data_int[DATA_ALL];
double data_double[DATA_ALL];
const char* filename = "data_all.txt";
int queue_first=0;
int queue_last=0;
int worker_num = 0;

pthread_mutex_t lock;
pthread_cond_t sch_cond;

void generate_data(const char*,int);
int queue_enqueue(int,int);
int check_is_order();
__time_t ClockGetTime();
void* scheduler();
struct queue_node* queue_dequeue(); 

void main()
{
	int int_or_double=1;
	//printf("please input an integer(0 is int; 1 is double):\n");
	//scanf("%d",&int_or_double);
	//printf("%d\n",int_or_double);
	generate_data(filename,int_or_double);
	__time_t t_begin = ClockGetTime();
	//init mutex and cond
	int error = 0;
	error += pthread_mutex_init(&lock,NULL);
	error += pthread_cond_init(&sch_cond,NULL);
	queue_enqueue(0,DATA_ALL-1);//put the whole data into queue
	if(error != 0)
	{
		printf("init has an error\n");
		exit(-1);
	}
	pthread_t sch_p;
	pthread_create(&sch_p,NULL,scheduler,NULL);
	pthread_join(sch_p,NULL);//wait until scheduler thread end
	//check if the data is in order
	int check_result;
	check_result = check_is_order();
	if(check_result == 1)
		printf("all the data are in order\n");
	else
		printf("there are some errors in order\n");
	__time_t t_end = ClockGetTime();
	__time_t t_use = (t_end - t_begin);
	printf("use time: %ld ms\n", t_use);
}

int comp(const void *a, const void *b){
    const double * pa = (const double *)a;
    const double * pb = (const double *)b;
    return (*pa<*pb)?-1:(*pa >*pb);
}

void swap(int k,int h)
{
	double t = data_double[k];
	data_double[k] = data_double[h];
	data_double[h] = t;
}


void* worktime(void* param)
{
	struct queue_node* q_n = (struct queue_node*) param;
	int left = q_n->begin;
	int right = q_n->end;
	if(right - left <= SORT_MIN_NUM)
	{
		qsort(&data_double[left],right - left + 1,sizeof(double),comp);
		printf("qsort locked worktime%d,%d,worker_num:%d\n",left,right,worker_num);
		pthread_mutex_lock(&lock);//before queue, lock;
		printf("qsort worktime enter lock %d,%d,worker_time:%d\n",left,right,worker_num);

	}
	else
	{
		int k = left;
		int h = right + 1;
		double pivot = data_double[left];
		while(1)
		{
			while(data_double[++k] < pivot)
				if (k == right)
					break;
			while(data_double[--h] > pivot)
				if(h == left)
					break;
			if(k >= h)
				break;
			swap(k,h);
		}
		swap(left,h);
		printf("qsort locked worktime%d,%d,worker_num:%d\n",left,right,worker_num);
		pthread_mutex_lock(&lock);//before queue, lock;
		printf("qsort worktime enter lock %d,%d,worker_time:%d\n",left,right,worker_num);
		queue_enqueue(left,h-1);
		queue_enqueue(h+1,right);
	}
	//worker finish work
	worker_num--;
	pthread_cond_signal(&sch_cond);
	pthread_mutex_unlock(&lock);
	printf("worktime leaving lock%d,%d,worker_num:%d\n",left,right,worker_num);
	pthread_exit(NULL);
}


void* scheduler()
{
	while(1)
	{
		pthread_mutex_lock(&lock);//lock scheduler
		if(queue_first == queue_last)
		{
			printf("queue is empty\n");
			if(worker_num == 0)
			{
				pthread_mutex_unlock(&lock);
				break;//if queue is empty and worker is zero,then finish
			}
			else
			{
				pthread_cond_wait(&sch_cond,&lock);
				//after thread end, unlock
				pthread_mutex_unlock(&lock);
			}
		}
		else if(worker_num < WORKER_MAX_NUM)//create new worker
		{
			struct queue_node* q_n = queue_dequeue();
			pthread_t new_p;
			worker_num++;
			printf("queue_first: %d, queue_last: %d, worker_num:%d\n",queue_first,queue_last,worker_num);
			pthread_create(&new_p,NULL,worktime,(void*)q_n);//create new thread
			pthread_mutex_unlock(&lock);
			pthread_detach(new_p);

		}
		else
		{
			printf("worker is full, is waiting....\n");
			pthread_cond_wait(&sch_cond,&lock);
			pthread_mutex_unlock(&lock);
		}
	}
}

struct queue_node* queue_dequeue()
{
	if(queue_last == queue_first)
		return NULL;
	else
	{
		queue_first++;
		return &queue_data[queue_first - 1];
	}
}


int check_is_order()
{
	for(int i = 0;i < DATA_ALL-1;i++)
	{
		if(data_double[i]>data_double[i+1])
			return 0;
	}
	return 1;
}



int queue_enqueue(int begin,int end)
{
	queue_data[queue_last].begin = begin;
	queue_data[queue_last].end = end;
	queue_last++;
	return (queue_last - 1);
}


void generate_data(const char* filename,int int_or_double)
{
	/*
	if(int_or_double == 0)
	{
		srand((int)time(0));
		for(int i = 0;i < DATA_ALL;i++)
			data_int[i] = random(DATA_MAX);
		//printf("%d\n",data_int[0]);
		FILE* f = fopen(filename,"w");
		if(f == NULL)
			printf("open fail..\n");
		for(int i = 0;i < DATA_ALL;i++)
		{
			fprintf(f,"%d ",data_int[i]);
			if(i%20 == 0)
				fprintf(f,"\n");
		}
		fclose(f);
	}
	else
	*/
	{
		srand((int)time(0));
		for(int i = 0;i < DATA_ALL;i++)
			data_double[i] = random(DATA_MAX)*1.0/3.14*3.1;
		FILE* f = fopen(filename,"w");
		if(f == NULL)
			printf("open fail..\n");
		for(int i = 0;i < DATA_ALL;i++)
		{
			fprintf(f,"%f ",data_double[i]);
			if(i%20 == 0)
				fprintf(f,"\n");
		}
		fclose(f);
	}

}

__time_t ClockGetTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (__time_t)ts.tv_sec * 1000000LL + (__time_t)ts.tv_nsec / 1000LL;
}























