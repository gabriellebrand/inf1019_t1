#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define DT 2

int main (int argc, char** argv) {
	int i, j, raj, pai;
	pai = getppid();
	
	printf("[PROG]Entrou no programa. argc = %d \n", argc);
	for (i=1; i<argc; i++) {
		printf("[PROG]print 1\n");
		raj = (int) strtol(argv[i], NULL, 10);
		printf("[PROG]print 2 %d\n", raj);
		for(j=0; j<raj; j++) {
			printf("[PROG] rajada %d: %d \n", i, getpid());
			sleep(DT);
			usleep(100); //escalonador nao consegue executar rapido o suficiente o sigstop
		}
		printf("[PROG]print 3\n");
		if(i != argc-1){
			kill(pai, SIGUSR1); //vai entrar em IO
			usleep(100); //escalonador nao consegue executar rapido o suficiente o sigstop
		//sleep(3*DT); //IO
		}
	}
	printf("[PROG] Acabou o processo %d, morre por favor\n", getpid());
	return 0;
}