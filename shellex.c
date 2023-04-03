/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
/*
ssh;CSE20201604@cspro.sogang.ac.kr
git;xitxxth
Command without fork, exec (cd, history) is designed as built-in function.
*/
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
//User defined
FILE* fp;
void pipe_handler(char **argv, int idx);
int pipe_counter(char **argv, int *arr);

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
    //user defined
    if(strncmp("!", cmdline, 1)!=0){//"!" never written
        char lastCmd[MAXLINE];
        fseek(fp, 0, SEEK_SET);//reset file cursor
        while((fgets(lastCmd, MAXLINE, fp))!=NULL) {}//to EOF
        if(strcmp(lastCmd, cmdline)!=0)//not repeated history
            fprintf(fp, "%s", cmdline);
        }//save cmd lines in history.txt



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
    //
    int status;//var for wait
    int pipe=0;//pipe flag, 0==off | 1==on
    int arr[MAXARGS];
    for(int i=0; i<MAXARGS; i++)    arr[i]=-1;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */


    int idx = pipe_counter(argv);
    for(int i=0; arr[i]>-1; i++){
        printf("Saved in %d\n",arr[i]);
    }
    printf("cnt: %d\n", idx);
    user defined execve
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = Fork())==0){//child
            execve(argv[0], argv, environ);//execute and dead
        }
        else{
            Wait(&status);
        }
    
        }


	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
	}
	else//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
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
        char strHistory[MAXLINE];//history string
        int i=1;//index for history list from 1 to ...
        fseek(fp, 0, SEEK_SET);//reset file cursor
        while((fgets(strHistory, MAXLINE, fp))!=NULL) {//to EOF
            printf("%d %s", i++, strHistory);//print history
        }
        fseek(fp, 0, SEEK_END);//move to EOF
        return 1;//pass execve
    }
    if(strncmp(argv[0], "!", 1)==0){//"!"
        if(!strcmp(argv[0], "!!")) {//"!!"
            char tmpCmd[MAXLINE];//for the last cmd
            fseek(fp, 0, SEEK_SET);//reset file cursor
            while((fgets(tmpCmd, MAXLINE, fp))!=NULL) {}//to EOF
            printf("last command is %s", tmpCmd);//print last cmd
            eval(tmpCmd);//execute it
            return 1;       
        }
        else{
            char tmpCmd[MAXLINE];//for the last cmd
            int num = atoi(argv[0]+1);//change string to num
            int foundFlag=1;//found? YES 1, NO 0
            fseek(fp, 0, SEEK_SET);//reset file cursor
            for(int i=0; i<num; i++){
                if(fgets(tmpCmd, MAXLINE, fp)==NULL){//not found
                    printf("-bash: !%d: event not found\n", num);
                    foundFlag=0;//flag off
                    break;
                }
            }
            fseek(fp, 0, SEEK_END);//move to EOF
            
            char lastCmd[MAXLINE];
            fseek(fp, 0, SEEK_SET);//reset file cursor
            while((fgets(lastCmd, MAXLINE, fp))!=NULL) {}//to EOF
            if(strcmp(lastCmd, tmpCmd)!=0)//not repeated history
                fprintf(fp, "%s", tmpCmd);//save cmd lines in history.txt
            if(foundFlag)   eval(tmpCmd);//run found execution
            return 1;
        }
    }

    if(strncmp("cd", argv[0], 2)==0){//"cd"
        if(argv[1]==NULL || *argv[1]=='~'){//cd, cd ~
            int set = chdir(getenv("HOME"));//cd home
            //var set has no role, mask warning message
        }
        else{
            if(chdir(argv[1])==-1){//chdir failed
                printf("No directory\n");//error message
            }
        }
        return 1;
    }
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

//proto-funciton for 1 | 1 | 1 ...
/*
void pipe_handler(char** argv, int idx)
{// handle mine >> | exists? >> pass it >> done , idx starts from 1
    int fd[2];
    pipe(fd);//commuicate with child of mine, fd[0] == read, fd[1] == write
    pid_t pid;           // Process id 
    int status;
    int pipe_flag=0; //pipe flag, child exists!
    // if(*(argv[idx+1])=='|'){
    //     pipe_flag=1;
    //     idx+=2;
    // }
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = Fork())==0){//child
            if(pipe_flag){
                dup2(fd[1], stdout);
                pipe_handler(argv, idx);
            }
            execve(argv[0], argv, environ);//execute and dead
        }
        else{
            if(pipe_flag){
                dup2(fd[0], stdin);
            }
            Waitpid(pid, &status, 0);
        }
    }
}
*/

int pipe_counter(char** argv)
{   
    int cnt=0, k=0;
    for(int i=0; argv[i]; i++){
        if(*(argv[i])=="|"){
            cnt++;
            arr[k++]=i;//arr[k] = | saved index
        }
    }
    return cnt;
}