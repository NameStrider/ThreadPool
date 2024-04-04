#pragma once

#include <stdio.h>
#include <pthread.h>

/**
 * �̳߳ع��ܣ��ػ��������ÿռ任ʱ�䡣Ԥ�ȴ�������̣߳�������ִ�й����ж�̬�����߳�����
 * �̳߳���ɣ��������(������-������ģ�ͣ�ͨ������������������ͬ��)�������̣߳�������߳�(�����߳�����)
 */

#define MAX_THREAD 8
#define TICK 2
#define COUNT 2

//����ṹ��
typedef struct task {
	void* (*func)(void*);
	void* arg;
} Task;

//�̳߳ؽṹ��
typedef struct threadPool {
	//�������(���ζ���)
	Task* taskQueue;
	int queueCapacity; //��������
	int queueSize; //��������
	int queueHead; //��ָ��
	int queueTail; //дָ��

	//�����߳�����(������)�ͼ�����߳�Id
	pthread_t* threadIds;
	pthread_t managerId;

	//�̳߳ؿ��Ʊ���
	int maxNum; //����������߳���
	int minNum; //��������С�߳���
	int liveNum; //����߳�����
	int busyNum; //æ�߳���
	int exitNum; //��־�������ڹ����߳��ж��Ƿ���Ҫ��ɱ
	int shutdown; //�Ƿ��˳���0--���˳���1--�˳�

	//�̻߳�����ͬ������
	pthread_mutex_t busyMutex; //�̳߳ز���(���������߳���������)���軥����
	pthread_mutex_t poolMutex; //��д busyNUm ���軥����(�൱�ڶ�д��)
	pthread_cond_t notEmpty; //ʹ����������ͬ�������ߺ�������
	pthread_cond_t notFull;
} ThreadPool;

//�ӿں���API
ThreadPool* ThreadPool_Create(int min, int max, int capasity);
void ThreadPool_Destroy(ThreadPool* pool);
void ThreadPool_AddTask(ThreadPool* pool, void* (*fun)(void*), void* args);
