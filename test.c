#include <stdlib.h>
#include "ThreadPool.h"

void* taskFunc(void* arg) {
	int i = *(int*)arg;
	int selfId = pthread_self();
	sleep(1);
	printf("sequence: %d, from thread %lu\n", i, selfId);
	return NULL;
}

int main() {
	ThreadPool* pool = ThreadPool_Create(2, 5, 10);
	if (!pool) {
		printf("fatal error\n");
		return -1;
	}

	for (int i = 0; i < 5; ++i) {
		int* data = (int*)malloc(sizeof(int));
		*data = i;
		ThreadPool_AddTask(pool, taskFunc, data);
	}

	sleep(2);

	for (int i = 5; i < 8; ++i) {
		int* data = (int*)malloc(sizeof(int));
		*data = i;
		ThreadPool_AddTask(pool, taskFunc, data);
	}

	sleep(10);
	ThreadPool_Destroy(pool);

	return 0;
}