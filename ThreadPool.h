#pragma once

#include <stdio.h>
#include <pthread.h>

/**
 * 线程池功能：池化技术，用空间换时间。预先创建多个线程，在任务执行过程中动态更新线程数量
 * 线程池组成：任务队列(生产者-消费者模型，通过互斥锁和条件变量同步)，工作线程，监管者线程(调整线程数量)
 */

#define MAX_THREAD 8
#define TICK 2
#define COUNT 2

//任务结构体
typedef struct task {
	void* (*func)(void*);
	void* arg;
} Task;

//线程池结构体
typedef struct threadPool {
	//任务队列(环形队列)
	Task* taskQueue;
	int queueCapacity; //队列容量
	int queueSize; //任务数量
	int queueHead; //读指针
	int queueTail; //写指针

	//工作线程数组(待创建)和监管者线程Id
	pthread_t* threadIds;
	pthread_t managerId;

	//线程池控制变量
	int maxNum; //常量，最大线程数
	int minNum; //常量，最小线程数
	int liveNum; //存活线程总数
	int busyNum; //忙线程数
	int exitNum; //标志变量用于工作线程判断是否需要自杀
	int shutdown; //是否退出，0--不退出，1--退出

	//线程互斥与同步机制
	pthread_mutex_t busyMutex; //线程池操作(增添任务，线程数量调整)所需互斥锁
	pthread_mutex_t poolMutex; //读写 busyNUm 所需互斥锁(相当于读写锁)
	pthread_cond_t notEmpty; //使用条件变量同步生产者和消费者
	pthread_cond_t notFull;
} ThreadPool;

//接口函数API
ThreadPool* ThreadPool_Create(int min, int max, int capasity);
void ThreadPool_Destroy(ThreadPool* pool);
void ThreadPool_AddTask(ThreadPool* pool, void* (*fun)(void*), void* args);
