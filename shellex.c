/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
//User defined
FILE* fp;
int mkdir_user();

int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    /*user defined code, for > history*/
    fp = fopen("history.txt", "a+t");//open history file if it exists or make a new history file
        if(!fp){
            printf("FILE OPEN ERROR\n");
            exit(0);//cannot use history tool, exit program
        }

    while (1) {
	/* Read */
	printf("> ");                   
	fgets(cmdline, MAXLINE, stdin); 
    fprintf(fp, "%s", cmdline);//save cmd lines in history.txt
	if (feof(stdin)){
        if(fclose(fp))
            printf("FILE CLOSE ERROR\n");
	    exit(0);
    }

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if (execve(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }

	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
	}
	else//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true    */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
	exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
    //user defined history
    /*by making a txt file for keeping history data
    we can use the txt file again after quit my shell*/
    if(!strcmp(argv[0], "history")){//history doesn't call fork, exec so we deal with it as built-in
        char strHistory[MAXLINE];
        int i=1;
        fseek(fp, 0, SEEK_SET);
        while((fgets(strHistory, MAXLINE, fp))!=NULL) {
            printf("%d %s" i++, strHistory);
        }
        fseek(fp, 0, SEEK_END);
        return 1;//pass execve
    }
    if(!strcmp(argv[0], "!!")){
        char tmpCmd[MAXLINE];
        fseek(fp, 0, SEEK_SET);
        while((fgets(tmpCmd, MAXLINE, fp))!=NULL) {}
        fseek(fp, 0, SEEK_END);
        printf("last command is %s", tmpCmd);
    }
    /*else if(!strcmp(*(argv[0]), "!")){

    }*/


    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


