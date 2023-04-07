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
pid_t pipe_handler(char **argv, int* arr, int idx, int *oldfd, int bg);
int pipe_counter(char **argv, int *arr);
void Quote_Killer(char* cmdline);
void Sigchld_handler(int s);
void Sigint_handler(int s);
void Sigtstp_handler(int s);
typedef struct{
    pid_t bgPid;
    char *bgSt;
    char bgCmd[MAXARGS];
} bgCon;
pid_t fgPgid;
bgCon bgCons[MAXARGS];
int bgNum, currNum;

int main() 
{
    sigset_t mask, prev;
    Signal(SIGCHLD, Sigchld_handler);
    Signal(SIGINT, Sigint_handler);
    Signal(SIGTSTP, Sigtstp_handler);
    bgNum=0;
    currNum=0;
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
    Quote_Killer(cmdline);
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

    int status;//var for wait
    int pipe=0;//pipe flag, 0==off | 1==o
    int oldfd=0;
    int arr[MAXARGS];
    arr[0]=-1;
    for(int i=1; i<MAXARGS; i++)    arr[i]=-2;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if(bg){
        for(int i=0; i<strlen(cmdline); i++)    if(cmdline[i]=='&') cmdline[i] = ' ';
        if(currNum==0)  bgNum=0;
        strcpy(bgCons[bgNum].bgCmd, cmdline);
        bgCons[bgNum].bgSt = "RUN";
        bgNum++;
        currNum++;
    }
    if (argv[0] == NULL)    return;   /* Ignore empty lines */

    int idx = pipe_counter(argv, arr);
    if((pid=Fork())==0){
        fgPgid = getpid();
        Setpgid(0, getpid());
        pipe_handler(argv, arr, 0, &oldfd, bg);
    }
    else    
        Waitpid(pid, &status, 0);
    
    bg=0;
    return;
}

/* If first arg is a builtin command, run it and return true    */
int builtin_command(char **argv) 
{
    int status;
    if (!strcmp(argv[0], "quit")){ /* quit command */
	printf("out\n");
    exit(0);
    }
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

    if(strcmp("cd", argv[0])==0){//"cd"
        if(argv[1]==NULL || *argv[1]=='~'){//cd, cd ~
            int set = chdir(getenv("HOME"));//cd home
            //var set has no role, mask warning message
        }
        else{
            if(chdir(argv[1])==-1){//chdir failed
                printf("No directory\n");//error messageS
            }
        }
        return 1;
    }
    if(strcmp("jobs", argv[0])==0){
        for(int i=0; i<bgNum; i++){
        if(strcmp("RUN", bgCons[i].bgSt)==0 || strcmp("STOP", bgCons[i].bgSt)==0)
            printf("[%d]\t%s\t%s", i, bgCons[i].bgSt , bgCons[i].bgCmd);
        }
        return 1;
    }
    if(strcmp("bg", argv[0])==0){
        int tarIdx = atoi(argv[1]);
        bgCons[tarIdx].bgSt ="RUN";
        kill(bgCons[tarIdx].bgPid, SIGCONT);
        return 1;
    }
    if(strcmp("fg", argv[0])==0){
        int tarIdx = atoi(argv[1]);
        bgCons[tarIdx].bgSt ="FG";
        kill(bgCons[tarIdx].bgPid, SIGCONT);
        Waitpid(bgCons[tarIdx].bgPid, &status, 0);
        return 1;
    }
    if(strcmp("kill", argv[0])==0){
        int tarIdx = atoi(argv[1]);
        bgCons[tarIdx].bgSt ="KILLED";
        kill(bgCons[tarIdx].bgPid, SIGKILL);//done?
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

pid_t pipe_handler(char** argv, int* arr, int idx, int *oldfd, int bg)
{// handle mine >> | exists? >> pass it >> done , idx starts from 1
    //printf("handler on! %d\n", idx);
    int fd[2];
    pid_t pid;           // Process id 
    int status;
    int pipe_flag=0; //pipe flag, child exists!
    int pipeStatus = pipe(fd);//commuicate with child of mine, fd[0] == read, fd[1] == write
    char *parsedArgv[4];//parsed argv
    //debug line
    for(int j=0; arr[j]!=-2; j++){
        //printf("arr[%d] is %d\n", j, arr[j]);
    }
    //printf("idx is %d\n", idx);
    //debug line

    int i, j=0;
    for(i=arr[idx]+1; argv[i]!=NULL && strcmp(argv[i], "|")!=0; i++, j++){
        //printf("i, j is %d, %d\n", i, j);
        parsedArgv[j] = argv[i];//strcpy(parsedArgv[j], argv[i]);
        //printf("copy!\n");
    }
    for(; j<4; j++)
        parsedArgv[j]=NULL;
    for(i=0; parsedArgv[i]; i++){
        //printf("idx:%d Pargv[%d]: %s\n", idx, i, parsedArgv[i]);
    }
    
    if(arr[idx+1] && arr[idx+1]>-1){
        pipe_flag=1;
        //printf("pipe_flag on!\n");
    }//problem!
    
    //printf("pipe passed\n");
    if (!builtin_command(parsedArgv)) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = Fork())==0){//child
            if(idx!=0 && *oldfd != STDIN_FILENO)   dup2(*oldfd, 0); //stdin-prev 
            if(pipe_flag){ // 1, 2, 3, ... nth cmd
                dup2(fd[1], 1);//stdout-pipe
                close(fd[1]);
            }
            if(execvp(parsedArgv[0], parsedArgv)<0) {
                    printf("%s:Command not found.\n", argv[0]);
                    exit(0);
            }
        }
            if(idx!=0) close(*oldfd);
            close(fd[1]);
            *oldfd = fd[0];
            if(pid>0)   Waitpid(pid, &status, 0);
            if(pipe_flag){
                pipe_handler(argv, arr, idx+1, oldfd, bg);
            }
    }
    exit(0);
}


int pipe_counter(char** argv, int* arr)
{   
    int cnt=0, k=1;
    for(int i=0; argv[i]; i++){
        if(strcmp(argv[i], "|")==0){
            cnt++;//the number of |
            arr[k++]=i;//arr[0]=-1, k>=1, arr[k] == kth | saved at ith (index)
        }
    }   
    return cnt;
}

void Quote_Killer(char* cmdline)
{
    for(int i=0; cmdline[i]; i++){
        if(cmdline[i] == '"' || cmdline[i] == '\'')   cmdline[i] = ' ';
    }
}

void Sigchld_handler(int s)
{
    int olderrno = errno;
    //pid = Waitpid(-1, NULL, 0);
    errno = olderrno;
}

void Sigint_handler(int s)
{
    int olderrno = errno;
    //pid = Waitpid(-1, NULL, 0);
    sio_puts("SIGINT\n");
    exit(0);
    errno = olderrno;
}

void Sigtstp_handler(int s)
{
    int olderrno = errno;
    Kill(-fgPgid, SIGSTOP);
    errno = olderrno;
}