#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ThreadPool.h"

void* manager(void* arg);
void* worker(void* arg);
void ThreadExit(ThreadPool* pool);

//创建线程池
ThreadPool* ThreadPool_Create(int min, int max, int capasity) {
	//1.参数检查
	if (min < 0 || max < 0 || min > max || max > MAX_THREAD) {
		printf("ThreadPool create args illegal\n");
		return NULL;
	}

	//2.分配线程池结构体
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	if (!pool) {
		printf("ThreadPool create fail\n");
		return NULL;
	}

	//3.初始化线程池属性
	pool->queueCapacity = capasity;
	pool->queueSize = 0;
	pool->queueHead = 0;
	pool->queueTail = 0;

	pool->minNum = min;
	pool->maxNum = max;
	pool->liveNum = min;
	pool->busyNum = 0;
	pool->exitNum = 0;

	pool->shutdown = 0;

	do {
		//4.创建任务队列
		pool->taskQueue = (Task*)malloc(sizeof(Task) * capasity);
		if (!pool->taskQueue) {
			printf("taskQueue create fail\n");
			break;
		}

		//5.分配工作线程数组(记录工作线程的tid)
		pool->threadIds = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (!pool->threadIds) {
			printf("threadIds malloc fail\n");
			break;
		}
		memset(pool->threadIds, -1, sizeof(pthread_t) * max); //数组必须初始化

		//6.初始化互斥锁和条件变量
		pthread_mutex_init(&pool->poolMutex, NULL);
		pthread_mutex_init(&pool->busyMutex, NULL);
		pthread_cond_init(&pool->notEmpty, NULL);
		pthread_cond_init(&pool->notFull, NULL);

		//7.创建管理者线程(准备工作做好最后创建线程)
		if (pthread_create(&pool->managerId, NULL, manager, pool) != 0) {
			printf("magager thread create fail\n");
			break;
		}	

		//8.创建工作线程
		for (int i = 0; i < min; ++i) {
			if (pthread_create(&pool->threadIds[i], NULL, worker, pool) != 0) {
				printf("magager thread create fail\n");
				break;
			}
		}

		return pool;
	} while (0);

	//集中释放内存，执行到此处说明线程池创建失败
	if (pool->taskQueue) free(pool->taskQueue);
	if (pool->threadIds) free(pool->threadIds);
	if (pool) free(pool);
	
	return NULL;
}

//管理者线程主函数，以一定的频率扫描线程池，动态增删线程
void* manager(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown) {
		//先sleep再判断是否调整线程数量，因为此时可能工作线程还没创建完
		sleep(TICK);

		pthread_mutex_lock(&pool->poolMutex);

		//是否增加线程
		if (pool->queueSize >= pool->liveNum && pool->liveNum <= pool->maxNum) { //注意越界

			//在数组中找到一个可用的tid作为线程创建参数
			int cnt = 0;
			for (int i = 0; i < pool->maxNum && cnt <= COUNT 
				&& pool->liveNum <= pool->maxNum; ++i) { //每次循环都要判断线程数量是否越界
				if (pool->threadIds[i] == -1) {
					if (pthread_create(&pool->threadIds[i], NULL, worker, pool) != 0) {
						printf("add threads fail\n");
						continue; //再试一次
					}
					else {
						//变量更新
						printf("add one thread\n");
						++pool->liveNum;
						++cnt;
					}
				}
			}
		}

		//是否删减线程
		if (pool->busyNum * 2 < pool->liveNum && pool->liveNum >= pool->minNum) { //注意越界

			/**
			 * what 让指定的线程退出
			 * who  因为没有任务而阻塞的线程(摸鱼的线程)
			 * how  阻塞有两种情况：作为消费者没有任务而阻塞，执行阻塞任务。无法区分，应该让摸鱼线程自杀
			 */
			printf("one thread need self-kill\n");
			pool->exitNum++; //标志变量+1，表示需要有线程退出
			pthread_cond_signal(&pool->notEmpty); //唤醒没有任务而阻塞线程让其判断是否需要自杀
		}

		pthread_mutex_unlock(&pool->poolMutex);
	}

	return NULL;
}

//工作线程主函数，消费者，不断从任务队列中取任务执行，没有就阻塞
void* worker(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;
	int selfId = pthread_self();
	Task task; //复用

	while (1) {
		pthread_mutex_lock(&pool->poolMutex); //对任务队列的操作是临界区

		//1.等待任务队列非空
		while (pool->queueSize == 0 && !pool->shutdown) { //while not if, wait until condition established

			//任务队列空，阻塞等待条件变量成立被唤醒
			printf("thread %lu blocked\n", selfId);
			pthread_cond_wait(&pool->notEmpty, &pool->poolMutex);

			//唤醒来源：监管者线程(可能自杀)，主线程(增加任务)，主线程(需要自杀，线程池销毁)
			//2.检查是否要自杀
			//这个自杀判断必须在 while 里面，假设执行这个函数的线程被管理线程发出的条件变量唤醒但是竞争
			//互斥锁失败，此时刚好一个线程从任务队列取完任务后解锁，且queueSize == 0，那么如果自杀判断在
			//while 外面就会继续阻塞
			if (pool->exitNum > 0) {
				pool->exitNum--; //不论是否真自杀都要更新
				if (pool->liveNum > pool->minNum) { //真自杀，保证不越界
					printf("thread need self-kill as manager ");
					pool->liveNum--;
					pthread_mutex_unlock(&pool->poolMutex); //避免死锁
					ThreadExit(pool); //线程销户
				}
			}		
		}

		//这个自杀判断必须在 while 外面，因为可能一开始 while 中断的原因就是 pool->shutdown != 0
		if (pool->shutdown != 0) {
			printf("thread need self-kill as ThreadPool destroyed ");
			pool->liveNum--;
			pthread_mutex_unlock(&pool->poolMutex);
			ThreadExit(pool); 
		}

		//3.取任务
		task = pool->taskQueue[pool->queueHead];

		//4.变量更新
		pool->queueHead = (pool->queueHead + 1) % pool->queueCapacity; //环形队列
		pool->queueSize--;
		pthread_mutex_lock(&pool->busyMutex);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->busyMutex);
		
		//5.通知生产者
		pthread_cond_signal(&pool->notFull);

		pthread_mutex_unlock(&pool->poolMutex);

		//6.执行任务
		task.func(task.arg);
		if (task.arg) free(task.arg);

		//7.变量更新
		pthread_mutex_lock(&pool->busyMutex);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->busyMutex);
	}

	return NULL;
}

//将 threadIds 数组对应的tid消除，线程销户
void ThreadExit(ThreadPool* pool) {
	int selfId = pthread_self();

	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIds[i] == selfId) {
			pool->threadIds[i] = -1; //"注销户口"
			break;
		}
	}

	printf("thread %lu exit\n", selfId);
	pthread_exit(NULL); //线程退出
}

//生产者，添加任务
void ThreadPool_AddTask(ThreadPool* pool, void* (*fun)(void*), void* args) {
	pthread_mutex_lock(&pool->poolMutex);

	//1.等待任务队列非满
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
		printf("taskQueue full\n");
		pthread_cond_wait(&pool->notFull, &pool->poolMutex); //唤醒后再次判断队列是否非满
	}

	//2.添加任务
	Task task;
	task.func = fun;
	task.arg = args;
	pool->taskQueue[pool->queueTail] = task;

	//3.变量更新
	pool->queueTail = (pool->queueTail + 1) % pool->queueCapacity;
	pool->queueSize++;

	//4.通知消费者
	pthread_cond_broadcast(&pool->notEmpty);

	pthread_mutex_unlock(&pool->poolMutex);
}

//线程池销毁
void ThreadPool_Destroy(ThreadPool* pool) {
	//1.设置标志变量
	pthread_mutex_lock(&pool->poolMutex);
	pool->shutdown = 1;
	pthread_mutex_unlock(&pool->poolMutex);

	//2.等待管理者线程退出
	pthread_join(pool->managerId, NULL);

	//3.通知因为没有任务而阻塞的线程自杀(!!BUG所有线程都要退出)
	for (int i = 0; i < pool->maxNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}

	//4.销毁资源
	pthread_mutex_destroy(&pool->poolMutex);
	pthread_mutex_destroy(&pool->busyMutex);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	//5.释放内存
	if (pool->taskQueue) free(pool->taskQueue);
	if (pool->threadIds) free(pool->threadIds);
	if (pool) free(pool);
}