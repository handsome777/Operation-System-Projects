#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

/***********************************************/
//predefine
#define COUNTER_NUM 5
#define CUSTOMER_NUM_MAX 500
/***********************************************/


/***********************************************/
//struct define
struct Counter
{
	pthread_t c_pthread_t;//pthread var
	pthread_mutex_t c_mutex_t;//mutex var
	pthread_cond_t c_cond_t;//cond var
};

struct Customer
{
	pthread_t cus_pthread_t;//pthread var
	pthread_mutex_t cus_mutex_t;//mutex var
	pthread_cond_t cus_cond_t;//cond var
	int cus_id;//customer id
	int coun_id;//counter id
	int enter_time;//enter time
	int wait_time;//serve time
};

struct queue_list
{
	int cus_id;
};

/***********************************************/

/***********************************************/
//predefine params
pthread_mutex_t que_lock;
pthread_mutex_t finish_cus_mutex_t;
pthread_mutex_t queue_t;
pthread_cond_t finish_cus_cond_t;
struct Counter counter[COUNTER_NUM];
struct Customer customer[CUSTOMER_NUM_MAX];
struct queue_list queue[CUSTOMER_NUM_MAX];
sem_t customer_wait;
int CUSTOMER_NUM = 0;
int finish_cus_num = 0;
int q_last = 0;
int q_first = 0;
const char* filename = "customer_data.dat";
struct timeval c_begin,c_end;
/***********************************************/

/***********************************************/
//function predefine
void init_counter();
void init_customer_();
void readf(const char*);
int queue_push(int);
void* counter_server(void*);
void* customer_server(void*);
struct queue_list* queue_pop();
double difftime(clock_t,clock_t);
/***********************************************/

int main()
{
	pthread_mutex_init(&que_lock,NULL);//create mutex for queue
	sem_init(&customer_wait,0,0);//create sem signal
	readf(filename);//read customer data from filename,and save in customer
	gettimeofday(&c_begin,NULL);
	init_counter();//init counter
	init_customer_();//init customer
	
	//init fi-mutex, queue-mutex and fi-cond
	pthread_mutex_init(&finish_cus_mutex_t,NULL);
	pthread_mutex_init(&queue_t,NULL);
	pthread_cond_init(&finish_cus_cond_t,NULL);

	pthread_mutex_lock(&finish_cus_mutex_t);//lock fi-mutex
	//printf("fin_mutex is locked\n");
	while(finish_cus_num != CUSTOMER_NUM)
	{
		//waiting a signal to continue the while
		//the signal must be released by counter(when a customer finished the server)
		pthread_cond_wait(&finish_cus_cond_t,&finish_cus_mutex_t);
	}
	pthread_mutex_unlock(&finish_cus_mutex_t);//unlock fi-mutex
	//printf("fin_mutex is unlocked\n");
	return 0;
}

/***********************************************/
//counter function
void* counter_server(void* id)
{
	int counter_id = id;
	int serve_time = 0;//every counter has a serve time,to record current time
	//printf("counter id:%d\n",counter_id);
	int start_time = 0;
	int end_time = 0;
	struct timeval start,end;
	int flag = 1;
	while(1)
	{
		if(flag == 1)
		{
			gettimeofday(&start,NULL);
			//printf("counter id:%d begin work at %d\n",counter_id,start);
			flag = 0;
		}
		sem_wait(&customer_wait);//if sem>0,sem-- and continue, if sem <=0,sutck in here until sem_post make sem > 0
		struct queue_list* q_next = queue_pop();//read first member in the queue
		//printf("counter_id:%d,qfirst:%d,qlast:%d\n",id,q_first,q_last);
		if(q_next == NULL)//if queue is NULL, continue waiting
		{
			//printf("no customer\n");
			continue;
		}
		if(customer[q_next->cus_id].coun_id != -1)//if customer has already served by a counter
		{
			//printf("customer id:%d  already have a counter\n",q_next->cus_id);
			continue;
		}
		//printf("customer id:%d in serve\n",q_next->cus_id);
		//now,first customer in the queue receive the serve by current counter
		pthread_mutex_lock(&customer[q_next->cus_id].cus_mutex_t);//lock customer, prevent it is served by other counter
		int current_cus_id = q_next->cus_id;//save the customer id
		customer[current_cus_id].coun_id = counter_id;//adjust customer's coun_id
		pthread_mutex_lock(&counter[counter_id].c_mutex_t);//lock counter, prevent it serve other customer
		//printf("counter id:%d lock customer id: %d\n",counter_id,current_cus_id);
		pthread_cond_signal(&customer[current_cus_id].cus_cond_t);//signal that customer,let it in serve
		pthread_mutex_unlock(&customer[current_cus_id].cus_mutex_t);//unlock customer
		//printf("counter id:%d unlock customer id: %d\n",counter_id,current_cus_id);
		pthread_cond_wait(&counter[counter_id].c_cond_t,&counter[counter_id].c_mutex_t);//if customer finish it serve, send a signal to counter
		//printf("customer id:%d server over\n",current_cus_id);
		gettimeofday(&end,NULL);
		pthread_mutex_unlock(&counter[counter_id].c_mutex_t);//customer serve finished, unlock counter
		end_time = (end.tv_usec-start.tv_usec)/1000;
		//printf("counter_id: %d,customer id:%d end time:%d\n",id,current_cus_id,end_time);
		start_time = end_time - customer[current_cus_id].wait_time;//compute finished serve time
		//time = enter_t + customer[current_cus_id].wait_time;
		printf("customer id:%d, enter at %d, served at %d, leave at %d,served by counter id:%d\n",(current_cus_id),customer[current_cus_id].enter_time,start_time,end_time,counter_id);
		
		//after serving, judging if there is no customer waiting in queue
		pthread_mutex_lock(&finish_cus_mutex_t);
		finish_cus_num++;//aftering a customer served, 
		pthread_cond_signal(&finish_cus_cond_t);
		pthread_mutex_unlock(&finish_cus_mutex_t);
		pthread_mutex_unlock(&counter[counter_id].c_mutex_t);//customer serve finished, unlock counter

	}	
}
/***********************************************/

/***********************************************/
//customer function
void* customer_server(void* customer_data)
{
	struct Customer* cus = (struct Customer*) customer_data;
	//printf("customer id:%d,enter time:%d\n",cus->cus_id,cus->enter_time);
    usleep(cus->enter_time*1000);
    //printf("customer id:%d,enter time:%d\n",cus->cus_id,cus->enter_time);

    pthread_cond_init(&customer[cus->cus_id].cus_cond_t,NULL);
    pthread_mutex_init(&customer[cus->cus_id].cus_mutex_t,NULL);

    pthread_cond_t* cus_cond_p = &customer[cus->cus_id].cus_cond_t;
    pthread_mutex_t* cus_mutex_p = &customer[cus->cus_id].cus_mutex_t;

    pthread_mutex_lock(cus_mutex_p);//lock customer
    int num = queue_push(cus->cus_id);
    sem_post(&customer_wait);
    //printf("customer id:%d is attempting arosing a counter\n",cus->cus_id);
    while(customer[cus->cus_id].coun_id == -1)//wait until a counter is unbusy
    {
    	//printf("customer id:%d is waiting\n",cus->cus_id);
    	pthread_cond_wait(cus_cond_p,cus_mutex_p);//wait a counter to arouse it
    }
    //printf("customer id:%d is aroused by counter id:%d\n",cus->cus_id,customer[cus->cus_id].coun_id);
    //a counter respond
    int counter_id = customer[cus->cus_id].coun_id;
    //printf("customer id:%d is svered by counter id:%d\n",cus->cus_id,counter_id);
    //printf("counter id %d serve customer id %d\n",counter_id,cus->cus_id);
    pthread_mutex_lock(&counter[counter_id].c_mutex_t);//lock counter
    usleep(customer[cus->cus_id].wait_time*1000);//stimulate serve
    pthread_cond_signal(&counter[counter_id].c_cond_t);
    
    pthread_mutex_unlock(&counter[counter_id].c_mutex_t);//unlock counter
    //printf("customer id:%d servering over\n",cus->cus_id);


    pthread_mutex_unlock(&(*cus_mutex_p));//unlock customer
}
/***********************************************/


/***********************************************/
//queue funtion
struct queue_list* queue_pop()
{
	pthread_mutex_lock(&queue_t);
	if(q_first == q_last)
	{
		pthread_mutex_unlock(&queue_t);
		return NULL;
	}
	else
	{
		q_first++;
		//printf("q_first:%d\n",q_first);
		pthread_mutex_unlock(&queue_t);
		return &queue[q_first-1];
	}
} 

int queue_push(int id)
{
	pthread_mutex_lock(&queue_t);
	queue[q_last].cus_id=id;
	q_last++;
	//printf("q_last:%d\n",q_last);
	pthread_mutex_unlock(&queue_t);
	return (q_last - 1);
}
/***********************************************/



/***********************************************/
//initial function
void readf(const char* filename)
{
	FILE* f = fopen(filename,"rb");
	int cus_id,enter_time,wait_time;
	
	while(fscanf(f,"%d %d %d\n",&cus_id,&enter_time,&wait_time) != EOF)
	{
		//printf("CUSTOMER_NUM:%d\n",CUSTOMER_NUM);
		customer[CUSTOMER_NUM].cus_id = cus_id;
		customer[CUSTOMER_NUM].enter_time = enter_time;
		customer[CUSTOMER_NUM].wait_time = wait_time;
		customer[CUSTOMER_NUM].coun_id = -1;
		printf("CUSTOMER_NUM:%d,enter_time%d,wait time%d,coun id: %d\n",cus_id,enter_time,wait_time,customer[CUSTOMER_NUM].coun_id);
		CUSTOMER_NUM++;
		
	}
	printf("load data successfully\n");
}

void init_customer_()
{
	int flag_error = 0;
	for(int i = 0;i < CUSTOMER_NUM;i++)
	{
		flag_error = pthread_create(&customer[i].cus_pthread_t,NULL,customer_server,(void*)&customer[i]);
		flag_error = pthread_mutex_init(&customer[i].cus_mutex_t,NULL);
		flag_error = pthread_cond_init(&customer[i].cus_cond_t,NULL);
	}
	if(flag_error == 0)
	{
		//printf("successfully init counter\n");
	}
	else
	{
		printf("init customer with error\n");
		exit(-1);
	}
}

void init_counter()//initial counter
{
	int flag_error = 0;
	for(int i = 0;i < COUNTER_NUM;i++)
	{
		
		flag_error = pthread_mutex_init(&counter[i].c_mutex_t,NULL);
		flag_error = pthread_cond_init(&counter[i].c_cond_t,NULL);
		flag_error = pthread_create(&counter[i].c_pthread_t,NULL,counter_server,(void*)i);
	}
	if(flag_error == 0)
		{
			//printf("successfully init counter\n");
		}
	else
	{
		printf("init counter with error\n");
		exit(-1);
	}
}
/***********************************************/


