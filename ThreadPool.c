#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ThreadPool.h"

void* manager(void* arg);
void* worker(void* arg);
void ThreadExit(ThreadPool* pool);

//�����̳߳�
ThreadPool* ThreadPool_Create(int min, int max, int capasity) {
	//1.�������
	if (min < 0 || max < 0 || min > max || max > MAX_THREAD) {
		printf("ThreadPool create args illegal\n");
		return NULL;
	}

	//2.�����̳߳ؽṹ��
	ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	if (!pool) {
		printf("ThreadPool create fail\n");
		return NULL;
	}

	//3.��ʼ���̳߳�����
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
		//4.�����������
		pool->taskQueue = (Task*)malloc(sizeof(Task) * capasity);
		if (!pool->taskQueue) {
			printf("taskQueue create fail\n");
			break;
		}

		//5.���乤���߳�����(��¼�����̵߳�tid)
		pool->threadIds = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (!pool->threadIds) {
			printf("threadIds malloc fail\n");
			break;
		}
		memset(pool->threadIds, -1, sizeof(pthread_t) * max); //��������ʼ��

		//6.��ʼ������������������
		pthread_mutex_init(&pool->poolMutex, NULL);
		pthread_mutex_init(&pool->busyMutex, NULL);
		pthread_cond_init(&pool->notEmpty, NULL);
		pthread_cond_init(&pool->notFull, NULL);

		//7.�����������߳�(׼������������󴴽��߳�)
		if (pthread_create(&pool->managerId, NULL, manager, pool) != 0) {
			printf("magager thread create fail\n");
			break;
		}	

		//8.���������߳�
		for (int i = 0; i < min; ++i) {
			if (pthread_create(&pool->threadIds[i], NULL, worker, pool) != 0) {
				printf("magager thread create fail\n");
				break;
			}
		}

		return pool;
	} while (0);

	//�����ͷ��ڴ棬ִ�е��˴�˵���̳߳ش���ʧ��
	if (pool->taskQueue) free(pool->taskQueue);
	if (pool->threadIds) free(pool->threadIds);
	if (pool) free(pool);
	
	return NULL;
}

//�������߳�����������һ����Ƶ��ɨ���̳߳أ���̬��ɾ�߳�
void* manager(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;

	while (!pool->shutdown) {
		//��sleep���ж��Ƿ�����߳���������Ϊ��ʱ���ܹ����̻߳�û������
		sleep(TICK);

		pthread_mutex_lock(&pool->poolMutex);

		//�Ƿ������߳�
		if (pool->queueSize >= pool->liveNum && pool->liveNum <= pool->maxNum) { //ע��Խ��

			//���������ҵ�һ�����õ�tid��Ϊ�̴߳�������
			int cnt = 0;
			for (int i = 0; i < pool->maxNum && cnt <= COUNT 
				&& pool->liveNum <= pool->maxNum; ++i) { //ÿ��ѭ����Ҫ�ж��߳������Ƿ�Խ��
				if (pool->threadIds[i] == -1) {
					if (pthread_create(&pool->threadIds[i], NULL, worker, pool) != 0) {
						printf("add threads fail\n");
						continue; //����һ��
					}
					else {
						//��������
						printf("add one thread\n");
						++pool->liveNum;
						++cnt;
					}
				}
			}
		}

		//�Ƿ�ɾ���߳�
		if (pool->busyNum * 2 < pool->liveNum && pool->liveNum >= pool->minNum) { //ע��Խ��

			/**
			 * what ��ָ�����߳��˳�
			 * who  ��Ϊû��������������߳�(������߳�)
			 * how  �����������������Ϊ������û�������������ִ�����������޷����֣�Ӧ���������߳���ɱ
			 */
			printf("one thread need self-kill\n");
			pool->exitNum++; //��־����+1����ʾ��Ҫ���߳��˳�
			pthread_cond_signal(&pool->notEmpty); //����û������������߳������ж��Ƿ���Ҫ��ɱ
		}

		pthread_mutex_unlock(&pool->poolMutex);
	}

	return NULL;
}

//�����߳��������������ߣ����ϴ����������ȡ����ִ�У�û�о�����
void* worker(void* arg) {
	ThreadPool* pool = (ThreadPool*)arg;
	int selfId = pthread_self();
	Task task; //����

	while (1) {
		pthread_mutex_lock(&pool->poolMutex); //��������еĲ������ٽ���

		//1.�ȴ�������зǿ�
		while (pool->queueSize == 0 && !pool->shutdown) { //while not if, wait until condition established

			//������пգ������ȴ�������������������
			printf("thread %lu blocked\n", selfId);
			pthread_cond_wait(&pool->notEmpty, &pool->poolMutex);

			//������Դ��������߳�(������ɱ)�����߳�(��������)�����߳�(��Ҫ��ɱ���̳߳�����)
			//2.����Ƿ�Ҫ��ɱ
			//�����ɱ�жϱ����� while ���棬����ִ������������̱߳������̷߳����������������ѵ��Ǿ���
			//������ʧ�ܣ���ʱ�պ�һ���̴߳��������ȡ��������������queueSize == 0����ô�����ɱ�ж���
			//while ����ͻ��������
			if (pool->exitNum > 0) {
				pool->exitNum--; //�����Ƿ�����ɱ��Ҫ����
				if (pool->liveNum > pool->minNum) { //����ɱ����֤��Խ��
					printf("thread need self-kill as manager ");
					pool->liveNum--;
					pthread_mutex_unlock(&pool->poolMutex); //��������
					ThreadExit(pool); //�߳�����
				}
			}		
		}

		//�����ɱ�жϱ����� while ���棬��Ϊ����һ��ʼ while �жϵ�ԭ����� pool->shutdown != 0
		if (pool->shutdown != 0) {
			printf("thread need self-kill as ThreadPool destroyed ");
			pool->liveNum--;
			pthread_mutex_unlock(&pool->poolMutex);
			ThreadExit(pool); 
		}

		//3.ȡ����
		task = pool->taskQueue[pool->queueHead];

		//4.��������
		pool->queueHead = (pool->queueHead + 1) % pool->queueCapacity; //���ζ���
		pool->queueSize--;
		pthread_mutex_lock(&pool->busyMutex);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->busyMutex);
		
		//5.֪ͨ������
		pthread_cond_signal(&pool->notFull);

		pthread_mutex_unlock(&pool->poolMutex);

		//6.ִ������
		task.func(task.arg);
		if (task.arg) free(task.arg);

		//7.��������
		pthread_mutex_lock(&pool->busyMutex);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->busyMutex);
	}

	return NULL;
}

//�� threadIds �����Ӧ��tid�������߳�����
void ThreadExit(ThreadPool* pool) {
	int selfId = pthread_self();

	for (int i = 0; i < pool->maxNum; ++i) {
		if (pool->threadIds[i] == selfId) {
			pool->threadIds[i] = -1; //"ע������"
			break;
		}
	}

	printf("thread %lu exit\n", selfId);
	pthread_exit(NULL); //�߳��˳�
}

//�����ߣ��������
void ThreadPool_AddTask(ThreadPool* pool, void* (*fun)(void*), void* args) {
	pthread_mutex_lock(&pool->poolMutex);

	//1.�ȴ�������з���
	while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
		printf("taskQueue full\n");
		pthread_cond_wait(&pool->notFull, &pool->poolMutex); //���Ѻ��ٴ��ж϶����Ƿ����
	}

	//2.�������
	Task task;
	task.func = fun;
	task.arg = args;
	pool->taskQueue[pool->queueTail] = task;

	//3.��������
	pool->queueTail = (pool->queueTail + 1) % pool->queueCapacity;
	pool->queueSize++;

	//4.֪ͨ������
	pthread_cond_broadcast(&pool->notEmpty);

	pthread_mutex_unlock(&pool->poolMutex);
}

//�̳߳�����
void ThreadPool_Destroy(ThreadPool* pool) {
	//1.���ñ�־����
	pthread_mutex_lock(&pool->poolMutex);
	pool->shutdown = 1;
	pthread_mutex_unlock(&pool->poolMutex);

	//2.�ȴ��������߳��˳�
	pthread_join(pool->managerId, NULL);

	//3.֪ͨ��Ϊû��������������߳���ɱ(!!BUG�����̶߳�Ҫ�˳�)
	for (int i = 0; i < pool->maxNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}

	//4.������Դ
	pthread_mutex_destroy(&pool->poolMutex);
	pthread_mutex_destroy(&pool->busyMutex);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	//5.�ͷ��ڴ�
	if (pool->taskQueue) free(pool->taskQueue);
	if (pool->threadIds) free(pool->threadIds);
	if (pool) free(pool);
}