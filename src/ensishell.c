/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

//******************************************************************
//
// STRUCTURE DE LISTE DES PROCESSUS APPELES //
typedef struct listeProc{
	int PID;
	char *nom;
	struct listeProc *suiv;
} listeProc;

// FDS utilisé pour les in&out
int fds[2];


//******************************************************************
//
// IMPLEMENTATION DU ENSISHELL //

void terminate(char *line);

// Permet d'afficher la liste des processus en cours
// (Parcours de liste des processus demandés + suppression des processus terminé)  
void displayBG();

// Permet de libérer les éléments de la liste
void liberer(listeProc *list);

// Initialiser dans le main
// Utilisable par toutes les fonctions
listeProc *procBG;

// Execution d'une seule commande
int executerCMD(struct cmdline *l, int i, int in, int out);

// int i : correspond à la commande numéro i des pipes (probables)
int executer(char *line)
{
	/* Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */

	struct cmdline *l;
	/* parsecmd free line and set it up to 0 */
	l = parsecmd( & line);
	/* If input stream closed, normal termination */
	if (!l) {
		terminate(0);
	}

	if (l->err) {
		/* Syntax error, read another command */
		printf("error: %s\n", l->err);
		return 0;
	}

	if (l->in) printf("in: %s\n", l->in);
	if (l->out) printf("out: %s\n", l->out);

	/* Display each command of the pipe */
	/*
	for (int i=0; l->seq[i]!=0; i++) {
		char **cmd = l->seq[i];
		printf("seq[%d]: ", i);
		for (int j=0; cmd[j]!=0; j++) {
			printf("'%s' ", cmd[j]);

		}
		printf("\n");
	}*/

	/* Execution */
	// Initialisation
	int fds[2];
	// Question 6 //
	int in = (l->in)?open(l->in, O_RDONLY):0;
	int out;
	for (int i=0; l->seq[i]!=0; i++) {
		// Construction du pipe
		if (pipe(fds)==-1){
			perror("pipe");
			return (EXIT_FAILURE);
		}
		// Pipe or not ?
		if (l->seq[i+1] != NULL){
			// S'il y a un pipe
			out = fds[1];
		}else{
			out = (l->out)?open(l->out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO):1;
		}
		// Execution de la commande i
		if (executerCMD(l,i,in, out)==EXIT_FAILURE){
			return EXIT_FAILURE;
		}

		in = fds[0];

	}
	return EXIT_SUCCESS;
}

int executerCMD(struct cmdline *l, int i, int in, int out){

	//question 4 : Cas spécial du jobs
	//Test préliminaire si l'utilisateur n'a rien taper
	if(l->seq[i]==NULL){
		return (EXIT_SUCCESS);
	}
	if(strncmp(l->seq[i][0],"jobs",strlen(l->seq[i][0])) == 0){ 
		displayBG();
		return (EXIT_SUCCESS);
	}

	pid_t pidProg = fork();
	if (pidProg == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pidProg == 0) {		
		//processus fils
		if (in != 0){
			if(dup2(in, 0) == -1){
				perror("dup2");
				return (EXIT_FAILURE);
			}
			close(in);
		}
		if (out != 1){
			if(dup2(out, 1) == -1){
				perror("dup2");
				return (EXIT_FAILURE);
			}
			close(out);
		}

		// execution
		if (execvp(l->seq[i][0],l->seq[i]) == -1){
			// Si la commande n'existe pas ou autre
			perror("execvp");
			printf("Commande %s non trouvée\n",l->seq[i][0]);
			return(EXIT_FAILURE);
		}
	} else {
		if (in != 0){
			close(in);
		}
		if (out != 1){
			close(out);
		}
		//question 3
		if (l->bg) {
			printf("background %s (&)\n",l->seq[i][0]);
			// Insertion en tête
			listeProc *buf = malloc(sizeof(listeProc));
			if (buf == NULL){
				perror("malloc");
				return (EXIT_FAILURE);
			}
			buf -> PID = pidProg;
			buf->nom = malloc(strlen(l->seq[i][0])+1);
			if (buf->nom == NULL){
				perror("malloc");
				return (EXIT_FAILURE);
			}
			strcpy(buf->nom, l->seq[i][0]); 
			buf -> suiv = procBG;
			procBG = buf;
			return (EXIT_SUCCESS);
		}

		//question 2, le processus père attend
		if (l->seq[i+1] == NULL){
			if (waitpid(pidProg, NULL,0) == -1){
				perror("waitpid");
				return(EXIT_FAILURE);
			}
		}
	}

	/* Remove this line when using parsecmd as it will free it */
	//free(line);

	return (EXIT_SUCCESS);
}

SCM executer_wrapper(SCM x)
{
	return scm_from_int(executer(scm_to_locale_stringn(x, 0)));
}
#endif

// IMPLEMENTATION DES PROTOTYPES //

void terminate(char *line) {
#ifdef USE_GNU_READLINE
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
		free(line);
	printf("exit\n");
	exit(0);
}

void displayBG() {
	listeProc *cour = procBG;
	listeProc *pred = NULL;
	while (cour->suiv != NULL){
		// Cas où le processus est terminé
		// On ne l'affiche pas et on le supprime de la liste
		if (waitpid(cour->PID, NULL,WNOHANG) != 0){
			listeProc *succ = cour->suiv;
			if(pred != NULL){
				pred->suiv = succ;
			}
			// Cas où on enlève le premier élément
			else{
				procBG = cour->suiv;
			}
			free(cour->nom);
			free(cour);
			cour = succ;
			
		}
		else{
			printf("PID : %d \t %s\n",cour->PID,cour->nom);
			pred = cour;
			cour = cour->suiv;
		}

	}
}

void liberer(listeProc *list){
	listeProc *cour = list;
	listeProc *succ = cour->suiv;
	while (succ != NULL){
		free(cour->nom);
		free(cour);
		cour = succ;
		succ = cour->suiv;

	}
}

/////////////////////////////////////////////////////////////////
//				MAIN
/////////////////////////////////////////////////////////////////

int main() {
	printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#ifdef USE_GUILE
	scm_init_guile();
	/* register "executer" function in scheme */
	scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	// Initialisation de la liste des processus en background//
	procBG = malloc(sizeof(listeProc));
	if (procBG == NULL){
		perror("malloc");
		return (EXIT_FAILURE);
	}
	procBG->PID = -1;
	procBG->nom = NULL;
	procBG->suiv = NULL;

	// Lecture des commandes utilisateurs

	while (1) {

		char *line=0;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#ifdef USE_GNU_READLINE
		add_history(line);
#endif


#ifdef USE_GUILE
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
			continue;
		}
#endif

		if(executer(line)==EXIT_FAILURE){
			exit(EXIT_FAILURE);
		}		

	}

	// Libération de la liste
	liberer(procBG);
}
