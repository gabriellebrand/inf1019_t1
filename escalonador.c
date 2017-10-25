#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
//#include <wait.h>

#define DT 2
#define MAXFILA 8
#define DESCE 1
#define SOBE -1
#define PERMANECE 0
#define SIGNEW 70

//./prog 1 2 3 NULL
typedef enum {
	PRONTO = 0,
	EXECUTANDO = 1,
	EM_ESPERA = 2,
	FINALIZADO = 3
} Estado;

typedef struct fila {
	int f[MAXFILA];
	int ini;
	int fim;
	int count;
} Fila;

typedef struct processo {
    int pid;
    int fila; //a fila em que o processo está atualmente (0, 1 ou 2, -1(IO))
    int tempoIO;
    Estado estado; //0 - pronto / 1 - executando / 2 - em espera
} Proc;

static Fila filas[3]; //F1 F2 F3
static Proc procs[MAXFILA]; //vetor com todos os processos que estao sendo escalonados
static Fila filaIO;
static int idxCorr = -1; //processo que esta atualmente sendo processado pela CPU
static int fAtual; //fila atual que está executando
static int quantumAtual; //tempo restante para acabar o quantum da fila atual
int fd[2];

void terminaIO();
void atualizaFilaIO();

void initFila(Fila *f) {
	f->ini = 0;
	f->fim = 0;
	f->count = 0;
}

void insereItem(Fila *fila, int val) {
	(fila->count)++;
	fila->f[fila->fim] = val;
	fila->fim = (fila->fim+1)%MAXFILA;
}

int removeItem(Fila *fila) {
	int valor = fila->f[fila->ini];
	fila->f[fila->ini] = 0;
	fila->ini = (fila->ini+1)%MAXFILA;
	(fila->count)--;
	
	return valor;
}

int filaCheia(Fila fila) {
	return (fila.count == MAXFILA);
}

int filaVazia(Fila fila) {
	return (fila.count == 0);
}

/*
static void printFila(Fila *fila) {
	int i;
	for(i=0;i<8;i++){
		printf("%d ",fila->f[i]);
	}
	printf("\n");
}*/

void initProc(Proc *proc, int pid) {
	proc->pid = pid;
	proc->fila = 0;
	proc->estado = FINALIZADO;
	proc->tempoIO = 0;
}


void novoProcCorrente();

void entraFila(int idx, int direcao) {
	procs[idx].fila = procs[idx].fila + direcao; //atualiza a fila atual do processo
	insereItem(&filas[procs[idx].fila], idx); //adiciona o processo na fila
}

void escalonaProcessos() {
	time_t ini;
	while (1) {
		printf("------------NOVO CLOCK-------------\n");
		ini = time(NULL);
		while(difftime(time(NULL),ini) < DT);
		//sleep(DT);
		
		//subtrair 1 segundo do quantum atual
		//TODO: verificar se teve chamada de IO entre essas linhas
		if (idxCorr == -1) {
			quantumAtual = 0;
			printf("[ESCAL] Nao ha processos para executar no momento\n");
		}
		else {
			quantumAtual -= DT;
			printf("[ESCAL] Subtrai quantum = %d\n", quantumAtual/DT);
		}
			
		//subtrair 1 segundo do tempo de IO de cada um dos processos que estao na fila de IO
		//checar quais processos da fila de IO chegaram ao tempo 0 de io
		//para o processo que chegou em tempo 0 de io, trocar seu status para pronto e adicionar na fila superior
		atualizaFilaIO();

		
		if(quantumAtual == 0) {
		
			if(idxCorr != -1) {
				//checar se o quantum se esgotou -> escalonar novo processo corrente
				printf("[ESCAL] Esgotou o quantum.\n");
				
				//pausa processo corrente
				kill(procs[idxCorr].pid, SIGSTOP);
				procs[idxCorr].estado = PRONTO;
		
				//adiciona o processo atual na fila inferior
				if (procs[idxCorr].fila == 2) { //se o processo já está na fila mais baixa possível, então continua na mesma
					entraFila(idxCorr, PERMANECE);
					printf("[ESCAL] Continuou na mesma prioridade idxCorr = %d\n", idxCorr);
				} 
				else
					entraFila(idxCorr, DESCE); //processo é rebaixado à fila inferior
					printf("[ESCAL] Diminuiu prioridade idxCorr = %d\n", idxCorr);
			}
			
			//seleciona um novo processo corrente;
			novoProcCorrente();
		}
	}

}

//varre todos os processos que estão em IO e subtrai 1 segundo
void atualizaFilaIO() {
	int i, c = 0, idxProc;
	
	if (filaVazia(filaIO)) {
		printf("[FILAIO]Fila de IO está vazia \n");
		return;
	}
	
	
	idxProc = filaIO.f[filaIO.ini];
	if(procs[idxProc].tempoIO == DT) { //terminou a IO, deve voltar pra fila de prontos
		printf("[FILAIO] Terminou IO idxProc = %d\n", idxProc);
		terminaIO();
	}
	
	for(i = filaIO.ini; c < filaIO.count; i = (i+1)%MAXFILA, c++) {
		idxProc = filaIO.f[i];
		(procs[idxProc].tempoIO) -= DT; //subtrai 1 dt do tempo em IO
		printf("[FILAIO] Subtraiu dt do idxProc = %d\n", idxProc);
	} 
	

}

/*
void subtraiRajadaProcCorr() {
	int resto;
	Fila *rajadas = &procs[idxCorr].rajadas;
	resto = rajadas->f[rajadas->ini];
	if (resto > 1) {
		resto--; //subtrai 1 segundo da rajada atual
		rajadas->f[rajadas->ini] = resto; //atualiza a rajada atual do processo com o tempo restante
	}
	else {
		printf("rajadas chegaram ao fim -> deveria ter sido removida pelo handler.\n");
	}
}*/

void executaProcCorrente() {
	printf("[EXECORR]Executa processo corrente. idxCorr = %d \n", idxCorr);
	
	//muda o estado do processo para executando
	procs[idxCorr].estado = EXECUTANDO;
	//executa processo
	kill(procs[idxCorr].pid, SIGCONT);

}

void novoProcCorrente() {
	printf("[NOVOCORR]Novo processo corrente. \n");
	if (!filaVazia(filas[0])){
		fAtual = 0;
		quantumAtual = DT;
	}
	else if (!filaVazia(filas[1])){
		fAtual = 1;
		quantumAtual = 2*DT;
	}
	else if (!filaVazia(filas[2])){
		fAtual = 2;
		quantumAtual = 4*DT;
	}
	else {
		//filas se esgotaram -> todos os processos terminaram
		printf("[NOVOCORR] Nao ha processo disponivel para executar. \n");
		idxCorr = -1;
		return;
	}
	printf("[NOVOCORR]quantum atual = %d fila atual = %d\n", quantumAtual/DT, fAtual);
	
	idxCorr = removeItem(&filas[fAtual]); //atualiza o indice do processo corrente
	printf("[NOVOCORR]indice do processo corrente: %d \n", idxCorr);
	//coloca o novo processo corrente em execução
	
	executaProcCorrente();
}

//faz o tratamento do fim da IO de um processo
void terminaIO() {
	//um processo filho terminou sua IO -> quem terminou o IO foi o primeiro processo da fila de IO (pois todos os tempos de IO sao iguais)
	int idx;
	
	if (filaVazia(filaIO)) {
		printf("\nErro: fila de IO esta vazia.\n");
		return;
	}

	//remove o indice do processo da fila de IO
	idx = removeItem(&filaIO);

	//dá sigstop para ele não começar a executar após sair do io
	kill(procs[idx].pid, SIGSTOP);

	procs[idx].estado = PRONTO; //muda o estado do processo para pronto
	
	//adiciona o processo na fila superior
	if (procs[idx].fila == 0) { //se o processo já está na fila mais prioritária possível, entao continua na mesma
		entraFila(idx, PERMANECE);
		printf("[ENDIO] Continua na mesma fila idx = %d\n", idx);
		return;
	}
	entraFila(idx, SOBE); //processo é promovido à fila superior
	printf("[ENDIO] Aumentou prioridade idx = %d\n", idx);
}

//handler do sigchild (enviado ao fim do programa) -> processo corrente é eliminado pelo pai
void sigchildHandler (int signal) {
	int status;
	waitpid(procs[idxCorr].pid, &status, WNOHANG);
	if (WIFEXITED(status) == 1) { //processo terminou
		printf("[SIGCHLD] idxCorr = %d\n", idxCorr);
		procs[idxCorr].estado = FINALIZADO;
		novoProcCorrente();
	}
}

//handler do sinal que informa que o processo corrente entrou em IO
void w4IOHandler(int signal) {
	//se o sinal foi enviado, entao o processo corrente entrou em IO antes do fim do quantum
	//adiciona o processo na fila de processos que estão em espera de IO
	kill(procs[idxCorr].pid, SIGSTOP);
	printf("[w4IO]Parou o processo que entrou em IO idx = %d\n", idxCorr);
	procs[idxCorr].tempoIO = 3*DT;
	procs[idxCorr].estado = EM_ESPERA;
	insereItem(&filaIO, idxCorr); 
	
	//seleciona um novo processo corrente
	novoProcCorrente();
}

void printArgs(char **argv) {
	int i;
	
	printf("\nargv: ");
	for(i=0;argv[i] != NULL; i++)
		printf("%s ", argv[i]);
	printf("\n");
}

int procuraProcLivre() {
	int i;
	for(i=0;i<MAXFILA;i++) {
		if (procs[i].estado == FINALIZADO)
			return i;
	}
	return -1;
}

void criaProcesso(char **argv) {
	int idx, pid;
	
	//se nao tiver espaço disponivel no vetor de processos, nao eh possivel criar um novo
	if ((idx = procuraProcLivre()) < 0) {
		printf("ERRO: lista de processos esta cheia. \n");
		return;
	}
	pid = fork();
	if (pid == 0) {
		printf("----- FORK de novo processo. PID: %d \n", getpid());
		raise(SIGSTOP);
		printf("[PROG]voltou do raise sigstop\n");

		printf("[PROG]processo %d vai começar.\n", getpid());
		printArgs(argv);
		
		if (execv(argv[0], argv) < 0)
			printf("[FORK] Erro ao executar processo: errno = %d \n", errno);
	} else {
		waitpid(pid, NULL, WUNTRACED); // wait until the child is stopped
		printf("processo pai vai adicionar na fila\n");
		procs[idx].estado = PRONTO;
		procs[idx].pid = pid;
		insereItem(&filas[0], idx);
		printf("inseriu novo processo de idx = %d\n", idx);
	}
}

void handlerNovoProc(int signal) {
	//char **str;
	char **argv, *w, *dado;
	int size, n,i;
	printf("[NovoProc]\n");
	
	close(fd[1]);
	
	//leitura do tamanho da string de args
	read(fd[0], &size, sizeof(size));
	
		//inicializa um vetor do tamanho da string de args
	dado = (char*)malloc(sizeof(char)*size);
	
	//leitura dos dados
	read(fd[0], dado, sizeof(char)*size);

	//leitura da quantidade args (nome do programa + rajadas)
	read(fd[0], &n, sizeof(int));
	
	argv = (char**)malloc(sizeof(char*)*(n+1));

	//separa os parametros recebidos
	w = strtok(dado," ");
	for(i=0; w != '\0'; i++) {
		argv[i] = w;
		w = strtok(NULL," ");
	}
	argv[i] = NULL;
	
	//for (i=0; i<n+1; i++)
		//printf("argv[%d]: %s\n", i, argv[i]);
	
	criaProcesso(argv);
}



/********************* INTERPRETADOR **********************/


static int ehNum (char c) //retorna 1 se o character está entre 0 e 9
{
	if (c>='0'&&c<='9')
		return 1;
	return 0;
}
//exec prog (1,2,3)
static char* leInput (int *numarg)
{
	char raw[1000], *str, c=0;
	int i, j;

	printf("Esperando input\n");
	while (c!='e') //Acha o primeiro caracter do comando
		scanf("%c", &c);

	scanf("%[^\n]%*c", raw);
	str = (char*) malloc (strlen(raw)); //limite superior de tamanho
	i = 4; //i está no endereco do programa a executar
	
	for (j=0; raw[i]!=' '; i++, j++)
		str[j]=raw[i]; //copia o endereco a executar para a string tratada

	*numarg=1; //Primeiro argumento foi colocado
	str[j++]=' ';

	while (raw[i]!='\0')
	{
		for (; !ehNum(raw[i]) && raw[i]!=')'; i++); //chegou num numero ou no fim das rajadas
		if (raw[i]==')') //se chegou no fim, sai do loop
			break;
		for (; ehNum(raw[i]); j++, i++) //enquanto estiver no numero
			str[j]=raw[i]; //copia o numero pra string tratada
		(*numarg)++;
		str[j++]=' ';
	}
	str[j]='\0';
	printf("Terminou de ler input\n");

	printf("str tratado: %s, numarg: %d\n", str, *numarg);

	return str;
}

void sizeOfParams(char **str) {
	int i;
	printf("entrou em sizeof \n");
	for (i=0;str[i]!=NULL;i++) {
		printf("elemento = %s size = %lu\n", str[i], strlen(str[i]));
	}
	printf("size of str = %lu\n", sizeof(str));
}

void mandaInput (char* str, int argc)
{
//	char *s = (char*) malloc (sizeof(char*)*3);
	//sizeOfParams(str);
	int size = strlen(str)+1;;
	printf("Vai enviar input fd = %d %d \n", fd[0], fd[1]);
	//primeiro envio: tamanho da string
	close(fd[0]);
	printf("fechou o fd de leitura\n");
	printf("Enviando: %s\n", str);
   write(fd[1], &size, sizeof(int));
   write(fd[1], str, size);
   write(fd[1], &argc, sizeof(int));
   printf("Enviou input \n");
   kill(getppid(), SIGUSR2);
	free(str);
}


void geraInterpretador () //Gera um interpretador e retorna o seu pid
{
	char* str;
	int argc, pid;
	pid = fork();
	if (pid==0) //interpretador
	{
		close(fd[0]); //n precisa ler
		while(1)
		{
			str = leInput (&argc);
			mandaInput(str, argc);
		}
		exit(1);
	}
	else //escalonador
	{
		close(fd[1]); //n precisa escrever
	}
}


int main() {
	int i;
	signal(SIGUSR1, w4IOHandler);
	signal(SIGCHLD, sigchildHandler);
	signal(SIGUSR2, handlerNovoProc);
	
	for(i=0;i<MAXFILA;i++) {
		initProc(&procs[i], 0);
	}
	
	pipe(fd);
	
	printf("inicio programa, fd = %d %d \n", fd[0], fd[1]);
	
	geraInterpretador();
	
	while(filaVazia(filas[0]));
	
	novoProcCorrente();
	escalonaProcessos();
	
	
	wait(NULL);
	return 0;
}