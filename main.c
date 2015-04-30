#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


void *rutina_fir1(void *params)
{
	
	return NULL;
}

char IP[16];	// adresa IP de cautare server web; se va incrementa iterativ

int main(int argc, char *argv[])
{
	pthread_t fir1;

	if(pthread_create(&fir1,NULL, &rutina_fir1, NULL)){
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}

	// asteptam incheierea
	if(pthread_join(fir1, NULL))
		perror("pthread_join");


	return EXIT_SUCCESS;
}
