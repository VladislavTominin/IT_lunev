#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "read_int.h"
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

typedef struct Thread_Information{
	pthread_t thread_id;
  double *S;
  double a, b, h;
  int num;
} ThreadInfo, *pThreadInfo;

typedef struct ProcessesNumbers{
	int coreid;
	int processnum;
  int physicalid;
} ProcNum, *pProcNum;

int *pexit_variable;

int Ncpus()
{
    FILE *cmd = popen("cat /proc/cpuinfo | grep -i 'core id'|sort | uniq | wc -l", "r");

    if (cmd == NULL)
        return -1;

    int nprocs;
    if (fscanf(cmd, "%d\n", &nprocs) != 1)
        return -1;

    pclose(cmd);
		//printf("%d\n",nprocs);
    return nprocs;
}

int Nlogproc()
{
    FILE *cmd = popen("cat /proc/cpuinfo | grep -i 'processor'|sort | uniq | wc -l", "r");

    if (cmd == NULL)
        return -1;

    int nproc;
    if (fscanf(cmd, "%d\n", &nproc) != 1)
        return -1;

    pclose(cmd);
		printf("Logical processes %d\n",nproc);
    return nproc;
}

int *GetNumberProc() {
    int procnum;
    int coreid;
    int physicalid;
    if((procnum = Nlogproc()) < 0 )
      return NULL;
    //printf("The number of logical processes :%d\n", procnum);
		int *mass;
		if((mass = (int *) malloc(sizeof(int)*procnum)) == NULL)
			return NULL;
    pProcNum pm;
    if((pm = (pProcNum) malloc(sizeof(ProcNum)*procnum)) == NULL)
      return NULL;
    FILE *cmd1 = popen("cat /proc/cpuinfo | egrep \"core id\" | grep -o -E \'[0-9]+\'", "r");
		FILE *cmd2 = popen("cat /proc/cpuinfo | egrep \"processor\" | grep -o -E \'[0-9]+\'", "r");
    FILE *cmd3 = popen("cat /proc/cpuinfo | egrep \"physical id\" | grep -o -E \'[0-9]+\'", "r");
    //m is a number of added processes to a massive
		int i = 0, j, m = 0;
		while ((fscanf(cmd1, "%d\n", &coreid)) != EOF && (fscanf(cmd2, "%d\n", &procnum) != EOF) && (fscanf(cmd3, "%d\n", &physicalid) != EOF))
		{
      pm[i].coreid = coreid;
      pm[i].processnum = procnum;
      pm[i].physicalid = physicalid;
      for(j = 0; j < i; ++j)
      {
        if(pm[i].coreid == pm[j].coreid && pm[i].physicalid == pm[j].physicalid)
          break;
      }
      //then previous cores not == current, put procnum
      if(i == j)
      {
          mass[m] = procnum;
          printf("Core id %d process %d physical id %d\n", coreid, procnum, physicalid);
          ++m;
      }
			++i;
		}
    pclose(cmd1);
		pclose(cmd2);
    pclose(cmd3);
    free(pm);
    return mass;
}

double F(double x)
{
    double f;
    f = 1/(1+x*x) + 1/(2+x*x);
    return f;
}

void *thread_function(void *arg)
{
  int s, j;
  cpu_set_t cpuset;
  pthread_t thread_id;
  pThreadInfo thread = (pThreadInfo) arg;
  thread_id = thread->thread_id;

   /* Set affinity mask to include CPUs = k-1 */

  CPU_ZERO(&cpuset);
  CPU_SET(thread->num, &cpuset);
  s = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    printf("Error during set affinity %d\n", thread->num);
  int i = 0;
  double S = 0;
  double a = thread->a, b = thread->b, h = thread->h;
  double cur = a;
  S += F(cur);
  cur += h;
  while (cur + h < b) {
    S += 2*F(cur);
    cur += h;
    if(i % 300 == 0)
    {
		if(*(pexit_variable) == 1)
			return NULL;
		
	}
		
    ++i;
  }

  //add the last with coef 1
  S += F(cur);
  cur += h;
  S = h*S/2;
  *(thread->S) = S;
	//printf("S is %f\n", S);
	//printf("a %f cur %f b %f S %f\n", a,cur+h, b, S);
	return (void *) thread;
}


void *thread_empty_function(void *arg)
{
  int s, j;
  cpu_set_t cpuset;
  pthread_t thread_id;
  pThreadInfo thread = (pThreadInfo) arg;
  thread_id = thread->thread_id;

   /* Set affinity mask to include CPUs = k-1 */

  CPU_ZERO(&cpuset);
  CPU_SET(thread->num, &cpuset);
  s = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    printf("Error during set affinity %d\n", thread->num);

  double S = 0;
  double a = thread->a, b = thread->b, h = thread->h;
  double cur = a;
  S += F(cur);
  cur += h;
  while (cur + h < b) {
    S += 2*F(cur);
    cur += h;
  }

  //add the last with coef 1
  S += F(cur);
  cur += h;
  S = h*S/2;
	//printf("a %f cur %f b %f S %f\n", a,cur+h, b, S);
	return (void *) thread;
}



double ParallelIntegral(double a, double b, double h, int *exit, int ncpus)
{
		pexit_variable = exit;
    //the number of cores (in several processes too)
		
    int n = ncpus, i;
		int *mass;
		if((mass = GetNumberProc()) == NULL)
			return -1;
    double *answer = (double *) malloc(sizeof(double)*(128*n + 2));
    //alloc the space for ThreadsInfo
		//printf("h = %f, N_per = %d\n", h, N_per);
  	pThreadInfo *Thread_Info = (pThreadInfo *) malloc(sizeof(pThreadInfo)*n);
  	for (i = 0; i < n; ++i)
  	{
  		pThreadInfo thread = (pThreadInfo) malloc(sizeof(ThreadInfo));
  		Thread_Info[i] = thread;
  	}
    int N_per = (int) ((b-a)/(h*n));
  	for (i = 0; i < n; ++i)
  	{
			printf("i cpus %d %d %d\n", mass[i%ncpus],  i/ncpus, mass[i%ncpus] + (i/ncpus)*ncpus);
  		pThreadInfo thread = Thread_Info[i];
  		thread->a = a + i * N_per * h;
      if(i != n - 1)
        thread->b = a + (i + 1) * N_per * h;
      else
        thread->b = b;
      thread->S = answer + 128*i;
      thread->num = mass[i%ncpus] + (i/ncpus)*ncpus;
      thread->h = h;
  		int e = pthread_create(&(Thread_Info[i]->thread_id), NULL, thread_function, Thread_Info[i]);
  		if (e == -1)
  		{
  			printf("Can't create thread :(\n");
  			return 0;
  		}
  		//printf("Create id : %ld\n", Thread_Info[i]->thread_id);
  	}

  	for (i = 0; i < n; ++i)
  	{
  		void *ret;
  		pthread_join(Thread_Info[i]->thread_id, &ret);
  		if(ret == NULL)
			goto END;
  		printf(" %d returned \n", ((pThreadInfo)ret) -> num);
  	}

    double S = 0;
    for(i = 0; i < n; ++i)
    {
      S += *(answer + i*128);
    }
    printf("Answer is %f\n", S);
    //free Thread_Information
    END : do{
  	for (i = 0; i < n; ++i)
  	{
  		free(Thread_Info[i]);
  	}
  	free(Thread_Info);
		free(mass);
  	return S;} while(0);
}
