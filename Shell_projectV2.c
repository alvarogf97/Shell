/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c
#include <string.h>
#include <errno.h>
#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

//listas para tareas (forma asincrona)
job * lista;
job * lista_suspendidos;
job * lista_terminados;
T_lista history;

// -----------------------------------------------------------------------
//                            prints lists
// -----------------------------------------------------------------------

void print_terminados(){
  job * tarea = lista_terminados->next;
  job * prev = lista_terminados;
  while(tarea != NULL){
    printf(CIAN"El proceso con pid: %i commando: %s ha finalizado\n", tarea->pgid, tarea->command);
    prev = tarea;
    tarea = tarea->next;
    prev = NULL;
    free(prev);
  }
  lista_terminados->next = NULL;
}

void print_suspendidos(){
  job * tarea = lista_suspendidos->next;
  job * prev = lista_suspendidos;
  while(tarea != NULL){
    printf(CIAN"El proceso con pid: %i commando: %s ha sido suspendido\n", tarea->pgid, tarea->command);
    prev = tarea;
    tarea = tarea->next;
    prev = NULL;
    free(prev);
  }
  lista_suspendidos->next = NULL;
}

// -----------------------------------------------------------------------
//                            handler_SIGCHLD
// -----------------------------------------------------------------------

void handler_SIGCHLD(int signal){
  block_SIGCHLD();
  int i;
  job * tarea;
  int status;
  enum status status_res;
  int info;
  for(i = 1; i<=list_size(lista); i++){
    tarea = get_item_bypos(lista,i);
    waitpid(tarea->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
		status_res = analyze_status(status,&info);
    if(status_res == EXITED || ( status_res == SIGNALED && info == SIGKILL)){
      add_job(lista_terminados,new_job(tarea->pgid,tarea->command,tarea->state));
      delete_job(lista,tarea);
      i--;
    }else if(status_res == SUSPENDED || (status_res == SIGNALED && info == SIGSTOP)){
      tarea->state = STOPPED;
      add_job(lista_suspendidos,new_job(tarea->pgid,tarea->command,tarea->state));
    }else if(status_res == SIGNALED && signal == SIGCONT){
      tarea->state = BACKGROUND;
    }
  }
  unblock_SIGCHLD();
}

// -----------------------------------------------------------------------
//                            cd
// -----------------------------------------------------------------------

void cd(char * directory){
  if(directory == NULL){
    chdir("/"); //directorio por defecto
  }else{
    chdir(directory);
  }
}

// -----------------------------------------------------------------------
//                            jobs
// -----------------------------------------------------------------------

void jobs(){
  block_SIGCHLD();
  print_job_list(lista);
  unblock_SIGCHLD();
}

// -----------------------------------------------------------------------
//                           fg
// -----------------------------------------------------------------------

void fg(char * args){
  //variables

  job * tareax;
  job * tarea;
  int info;
  int status;
  enum status status_res;
  int pos = 0;

  //buscamos Posicion

  if(args == NULL){
    if(empty_list(lista) == 1){
      printf(ROJO"no hay procesos\n");
    }
    pos = list_size(lista);
  }else if(atoi(args)>list_size(lista)){
    printf(ROJO"tarea no encontrada\n");
  }else{
    pos = atoi(args);
  }

  //metemos en FOREGROUND

  if(pos != 0){
    block_SIGCHLD();
    tareax = get_item_bypos(lista,pos);
    tarea = new_job(tareax->pgid,tareax->command,tareax->state); //clonamos la tarea
    block_SIGCHLD();
    delete_job(lista,tareax);//la eliminamos de los procesos
    unblock_SIGCHLD();

    set_terminal(tarea->pgid); //le damos la terminal
    if(tarea->state == STOPPED){
      killpg(tarea->pgid,SIGCONT); //señal para que continue
    }
    tarea->state = FOREGROUND; //la ponemos en primer plano
    waitpid(tarea->pgid,&status,WUNTRACED); //esperamos por ella
    set_terminal(getpid());
    status_res = analyze_status(status,	&info);
    if(status_res == SUSPENDED){ //si se vuelve a suspender
      tarea->state = STOPPED;
      printf(CIAN"SUSPENDED-> pid: %i, command %s\n",tarea->pgid, tarea->command);
      block_SIGCHLD();
      add_job(lista, tarea);
    }else{
      printf(VERDE"Foreground pid: %i, command: %s, %s, info: %i\n", tarea->pgid, tarea->command, status_strings[status_res], info);
    }
    unblock_SIGCHLD();
  }
}

// -----------------------------------------------------------------------
//                           bg
// -----------------------------------------------------------------------

void bg(char * args){
  block_SIGCHLD();
  job * tarea;

  if(args == NULL){
    printf(ROJO"Faltan argumentos\n");
  }else if (empty_list(lista) || atoi(args)>list_size(lista)) {
    printf(ROJO"Tarea no encontrada\n");
  }else{
    tarea = get_item_bypos(lista,atoi(args));
    tarea->state = BACKGROUND;
    unblock_SIGCHLD();
    killpg(tarea->pgid,SIGCONT);
  }
  unblock_SIGCHLD();
}

// -----------------------------------------------------------------------
//                           historial
// -----------------------------------------------------------------------

void historial(char * args1, char ** args, int * ok){
  int s;
  if(args1 == NULL){
    print_history(history);
    *ok = 0;
  }else{
    get_history_bypos(args, atoi(args1),history,&s);
    if(s == 0){
      *ok = 1;
    }else{
      printf(ROJO"Comando no encontrado\n");
      *ok = 0;
    }
  }
}

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

int main(void)
{
  int ok;
  history = new_history();
  lista = new_list("Shell");
  lista_terminados = new_list("exited");
  lista_suspendidos = new_list("suspended");

	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
  char *ar;
  char ** ar2;
  // probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */
  signal(SIGCHLD,handler_SIGCHLD);

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{
    block_SIGCHLD();
    print_terminados();
    print_suspendidos();
    unblock_SIGCHLD();
		printf(NORMAL"COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background,&history);  /* get next command */

		if(args[0]==NULL) continue;   // if empty command

    //añadimos al historial

    if (strcmp("historial",args[0]) == 0) {
      add_history(&history,args[0],args);
      historial(args[1],args,&ok);
      if(ok == 0){
        continue;
      }
    }
    add_history(&history,args[0],args);

    if(strcmp("cd",args[0]) == 0){
      cd(args[1]);
    }else if (strcmp("jobs",args[0]) == 0) {
      jobs();
    }else if (strcmp("fg",args[0]) == 0) {
      fg(args[1]);
    }else if (strcmp("bg",args[0]) == 0) {
      bg(args[1]);
    }else{
      ignore_terminal_signals();
      pid_fork = fork();
      if(pid_fork == 0){
        new_process_group(getpid());
        restore_terminal_signals();
        execvp(args[0],args);
        exit(-1);
      }else{
        block_SIGCHLD();
        if(background == 0){
          set_terminal(pid_fork);
          waitpid(pid_fork,&status,WUNTRACED);
          set_terminal(getpid());
          status_res = analyze_status(status,	&info);
          if(status_res == SUSPENDED){
            printf(CIAN"SUSPENDED-> pid: %i, command %s\n",pid_fork, args[0]);
            add_job(lista, new_job(pid_fork,args[0],STOPPED));
          }else if(info == 255){ //info number for error command not found
            printf(ROJO"Error, command not found: %s \n", args[0]);
          }else{
            printf(VERDE"Foreground pid: %i, command: %s, %s, info: %i\n", pid_fork, args[0], status_strings[status_res], info);
          }
          unblock_SIGCHLD();
        }else{
              block_SIGCHLD();
              printf(VERDE"Background job running... pid: %i, command: %s\n", pid_fork, args[0]);
              add_job(lista, new_job(pid_fork,args[0],BACKGROUND));
              unblock_SIGCHLD();
        }
      }
    }

	} // end while
}
