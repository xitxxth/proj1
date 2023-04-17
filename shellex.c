/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define EMPTY -1
#define RUN 1
#define STOP 0
#define MAXPROCESS 128
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
int pipe_counter(char **argv, int *arr);
void Quote_Killer(char* cmdline);
void Sigchld_handler(int s);
void Sigint_handler(int s);
void Sigtstp_handler(int s);
typedef struct{
    pid_t bgPid;
    int job_idx;
    int bgSt;
    char bgCmd[MAXARGS];
} bgCon;
pid_t fgPgid;
bgCon bgCons[MAXPROCESS];
int currNum, pidx;// pidx;index of jobs, currNum; current number of processses, bgNum; number of background jobs
void Init_job(bgCon* data);
void Add_job(bgCon* data, pid_t pid, int state, char* cmdline);
void Print_job(bgCon* data);
void JobStatus_change(bgCon* data, int job_idx);
void JobStatus_empty(bgCon* data, int job_idx);
void Wait_job(bgCon* data, int job_idx);
void JobStatus_stop(bgCon* data, int job_idx);
void JobStatus_run(bgCon* data, int job_idx);
void Run_job(bgCon* data, int job_idx);
void Kill_job(bgCon* data, int job_idx);
void pipe_handler(char **argv, int* arr, int idx, int *oldfd, int bg, char *cmdline, int job_idx);
void bg_pipe_handler(char **argv, int* arr, int idx, int *oldfd, int bg, char *cmdline, int job_idx);
int compare(const void* first, const void* second)
{
    return ((bgCon*)first)->job_idx - ((bgCon*)second)->job_idx;
}//FOR SORTING JOBS

int main() 
{
    //mainPid = getpid();
    sigset_t mask, prev;
    Signal(SIGINT, Sigint_handler);
    Signal(SIGTSTP, Sigtstp_handler);
    Signal(SIGCHLD, Sigchld_handler);
    pidx=-1;//JOB INDEX
    currNum=0;//JOB NUMBER
    Init_job(bgCons);
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
    int bg=0;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int oldfd=0;//previous file descriptor (will be used in pipe_handler, defined here for function argument)
    int arr[MAXARGS];//index of '|' saved
    arr[0]=-1;//Assume there exists '|' before first command
    for(int i=1; i<MAXARGS; i++)    arr[i]=-2;//initial value
    for(int i=0; i<strlen(cmdline); i++){
        if(cmdline[i]=='&'){
            cmdline[i] = ' ';
            bg = 1;//kill '&', turn on bg flag
        }
    } 
    strcpy(buf, cmdline);
    int trash = parseline(buf, argv);//trash, eliminate warning message
    if (argv[0] == NULL)    return;   /* Ignore empty lines */
    trash = pipe_counter(argv, arr); // trash
    if(!bg) fgPgid = (pidx+1);//if foreground, save job index(pidx) to fgPgid
    if(bg)  bg_pipe_handler(argv, arr, 0, &oldfd, bg ,cmdline, fgPgid);//call background pipe handling function
    else    pipe_handler(argv, arr, 0, &oldfd, bg, cmdline, fgPgid);//call foreground pipe handling function
    bg=0;//reset
    return;
}

/* If first arg is a builtin command, run it and return true    */
int builtin_command(char **argv) 
{
    int status;
    if(!strcmp(argv[0], "exit")){
    Kill(0 , SIGTERM);//off the shell
    exit(0);
    }
    if (!strcmp(argv[0], "quit")){ /* quit command */
    exit(0);//off the shell
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
            printf("%s", tmpCmd);//print last cmd
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
            if(foundFlag){
                printf("%s", tmpCmd);
                eval(tmpCmd);//run found execution
            }
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
        Print_job(bgCons);//call print function
        return 1;
    }
    if(strcmp("bg", argv[0])==0){
        char per_int[6];//temporary command line
        strcpy(per_int, argv[1]);//copy
        for(int i=0; i<6; i++)  if(per_int[i] == '%')   per_int[i] = '0';//kill %
        int tarIdx = atoi(per_int);//string %# -> int #
        int tmp=-1, i;//initial value
        for(i=0; i<MAXPROCESS; i++){
            if(bgCons[i].job_idx == tarIdx){//found
                if(bgCons[i].bgSt == -1 || bgCons[i].job_idx == -1){//if empty 
                    printf("No such job\n");//error
                    return 1;
                }
                tmp = i;//found!
            }
        }
        if(tmp==-1){//not found
            printf("No such job\n");//error
            return 1;
        }
        printf("[%d] running %s", bgCons[tmp].job_idx, bgCons[tmp].bgCmd);
        JobStatus_run(bgCons, tarIdx);//change job status into run
        Run_job(bgCons, tarIdx);//run job
        JobStatus_empty(bgCons, tarIdx);//change job status into run
        return 1;
    }
    if(strcmp("fg", argv[0])==0){
        char per_int[6];
        strcpy(per_int, argv[1]);
        for(int i=0; i<6; i++)  if(per_int[i] == '%')   per_int[i] = '0';
        int tarIdx = atoi(per_int);
        int tmp=-1, i;
        for(i=0; i<MAXPROCESS; i++){
            if(bgCons[i].job_idx == tarIdx){
                if(bgCons[i].bgSt == -1 || bgCons[i].job_idx == -1){
                    printf("No such job\n");
                    return 1;
                }
                tmp = i;
            }
        }
        if(tmp==-1){
            printf("No such job\n");
            return 1;
        }
        printf("[%d] running %s", bgCons[tmp].job_idx, bgCons[tmp].bgCmd);
        /* SAME ABOVE UNTIL HERE*/
        fgPgid = tarIdx;//new foreground job
        JobStatus_run(bgCons, tarIdx);//SAME ABOVE
        Wait_job(bgCons, tarIdx);//must wait (fore ground)
        JobStatus_empty(bgCons, tarIdx);//SAME ABOVE
        return 1;
    }
    if(strcmp("kill", argv[0])==0){
        char per_int[6];
        strcpy(per_int, argv[1]);
        for(int i=0; i<6; i++)  if(per_int[i] == '%')   per_int[i] = '0';
        int tarIdx = atoi(per_int);
        int tmp=-1, i;
        for(i=0; i<MAXPROCESS; i++){
            if(bgCons[i].job_idx == tarIdx){
                if(bgCons[i].bgSt == -1 || bgCons[i].job_idx == -1){
                    printf("No such job\n");
                    return 1;
                }
                tmp = i;
            }
        }
        if(tmp==-1){
            printf("No such job\n");
            return 1;
        }
        /*SAME ABOVE UNTIL HERE*/
        Kill_job(bgCons, tarIdx);//SIGKILL JOB
        JobStatus_empty(bgCons, tarIdx);//SAME ABOVE
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

void pipe_handler(char** argv, int* arr, int idx, int *oldfd, int bg, char *cmdline, int job_idx)
{// handle mine >> | exists? >> pass it >> done , idx starts from 1
    //printf("handler on! %d\n", idx);
    int fd[2];//file descriptor
    pid_t pid;           // Process id 
    int status;//WAIT - STATUS
    int pipe_flag=0; //pipe flag, child exists!
    int pipeStatus = pipe(fd);//commuicate with child of mine, fd[0] == read, fd[1] == write
    char *parsedArgv[4];//parsed argv
    int i, j=0;
    int forked;
    for(i=arr[idx]+1; argv[i]!=NULL && strcmp(argv[i], "|")!=0; i++, j++){
        parsedArgv[j] = argv[i];//strcpy(parsedArgv[j], argv[i]);
    }
    for(; j<4; j++) parsedArgv[j]=NULL;//end argv with NULL
    if(arr[idx+1] && arr[idx+1]>-1) pipe_flag=1;//pipe exist flag
    
    
    if (!(forked=builtin_command(parsedArgv))) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = Fork())==0){//child
            printf("forked!!!\n");
                if(idx!=0 && *oldfd != STDIN_FILENO)   dup2(*oldfd, 0); //stdin-prev 
                if(pipe_flag){ // 1, 2, 3, ... nth cmd
                    dup2(fd[1], 1);//stdout-pipe
                    close(fd[1]);//close not used pipe
                }
                if(execvp(parsedArgv[0], parsedArgv)<0) {
                        printf("%s:Command not found.\n", argv[0]);
                        exit(0);
                }
            }
            if(idx==0)  pidx++;//job_idx ++
            if(idx!=0) close(*oldfd);//close previous one
            close(fd[1]);//close not used one
            *oldfd = fd[0]; //STDIN - PREVIOUS PIPE
            Add_job(bgCons, pid, 1, cmdline);//add to job table
            if(pipe_flag)   pipe_handler(argv, arr, idx+1, oldfd, bg, cmdline, job_idx);//call recursively
            if(pid>0 && forked!=1){
                                printf("forekd\n");
                Waitpid(pid, &status, WUNTRACED);//WAIT
            }
            if(pipe_flag){}//NONE
            else{
                if(WIFEXITED(status)){
                    JobStatus_empty(bgCons, job_idx);//change job status into empty
                    pidx--;//job_idx --
                }
                else if(WIFSTOPPED(status)) JobStatus_stop(bgCons, job_idx);//change job status into stop
            }
    }
    return;
}

void bg_pipe_handler(char **argv, int* arr, int idx, int *oldfd, int bg, char *cmdline, int job_idx)
{// handle mine >> | exists? >> pass it >> done , idx starts from 1
    //printf("handler on! %d\n", idx);
    int fd[2];
    pid_t pid;           // Process id 
    int status;
    int pipe_flag=0; //pipe flag, child exists!
    int pipeStatus = pipe(fd);//commuicate with child of mine, fd[0] == read, fd[1] == write
    char *parsedArgv[4];//parsed argv
    int i, j=0;
    int forked;
    for(i=arr[idx]+1; argv[i]!=NULL && strcmp(argv[i], "|")!=0; i++, j++){
        parsedArgv[j] = argv[i];//strcpy(parsedArgv[j], argv[i]);
    }
    for(; j<4; j++) parsedArgv[j]=NULL;//END ARGV WITH NULL
    if(arr[idx+1] && arr[idx+1]>-1) pipe_flag=1;//IS THERE '|' EXISTS?
    
    if (!(forked=builtin_command(parsedArgv))) { //quit -> exit(0), & -> ignore, other -> run
            if((pid = Fork())==0){//child
            if(idx!=0 && *oldfd != STDIN_FILENO)   dup2(*oldfd, 0); //stdin-prev 
            if(pipe_flag){ // 1, 2, 3, ... nth cmd
                dup2(fd[1], 1);//stdout-pipe
                close(fd[1]);//close not used pipe
            }
            if(execvp(parsedArgv[0], parsedArgv)<0) {
                    printf("%s:Command not found.\n", argv[0]);
                    exit(0);
            }
        }
            if(idx==0)  pidx++;//job_idx ++
            if(idx!=0) close(*oldfd);//close previous one
            close(fd[1]);//close not used one
            *oldfd = fd[0]; //STDIN - PREVIOUS PIPE
            Add_job(bgCons, pid, 1, cmdline);//add job to job table
            if(pipe_flag)   bg_pipe_handler(argv, arr, idx+1, oldfd, bg, cmdline, job_idx);//call recursively
    }
    return;
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
        if(cmdline[i] == '"' || cmdline[i] == '\'')   cmdline[i] = ' ';//KILL QUOTE
    }
}

void Sigchld_handler(int s)
{
    int olderrno = errno;
    pid_t pid;
    int target = -1;//INITIAL VALUE
    int status;//WAIT - STATUS
    while((pid=waitpid(-1, 0, WNOHANG))>0){//REAP ALL
        for(int i=0; i<MAXPROCESS; i++){
            if(bgCons[i].bgPid == pid){
                target = bgCons[i].job_idx;
                break;
            }//WHO IS REAPED?
        }
        for(int i=0; i<MAXPROCESS && target >-1; i++){
            if(bgCons[i].job_idx == target){
                JobStatus_empty(bgCons, target);
                target=-1;
                break;
            }//CLEAR THE REAPED-JOB TABLE
        }
    }
    errno = olderrno;
}

void Sigint_handler(int s)
{
    int olderrno = errno;
    for(int i=0; i<MAXPROCESS; i++){
        if(bgCons[i].job_idx == fgPgid){
            bgCons[i].bgSt = -1;
            bgCons[i].job_idx = -1;
            Kill(bgCons[i].bgPid, SIGTERM);
            currNum--;
        }//KILL FOREGROUND JOB & CLEAR JOB TABLE
    }
    printf("\n");
    errno = olderrno;
}

void Sigtstp_handler(int s)
{
    int olderrno = errno;
            for(int i=0; i<MAXPROCESS; i++){
            if(bgCons[i].job_idx == fgPgid){
                bgCons[i].bgSt = 0;
                break;
            }
        }//CHANGE JOB TABLE
        for(int i=0; i<MAXPROCESS; i++){
            if(bgCons[i].job_idx == fgPgid){
                bgCons[i].bgSt = 0;
                Kill(bgCons[i].bgPid, SIGSTOP);
                Kill(0, SIGCHLD); 
            }
        }//STOP FOREGROUND JOB
    printf("\n");
    errno = olderrno;
}

void Init_job(bgCon* data)
{
    for(int i=0; i<MAXPROCESS; i++){
        data[i].bgPid = 0;
        data[i].bgSt = -1;
        data[i].job_idx = -1;
    }//INITIALIZE JOB TABLE
}

void Add_job(bgCon* data, pid_t pid, int state, char* cmdline)
{
    if(currNum==MAXPROCESS){
        printf("PROCESS OVER\n");
        return;
    }//SIZE OVER
    int i;
    for(i=0; i<MAXPROCESS; i++){
        if(data[i].bgSt == -1){
            strcpy(data[i].bgCmd, cmdline);
            data[i].bgPid = pid;
            data[i].bgSt = state;
            data[i].job_idx = pidx;
            currNum++;
            return;
        }//ADD TO JOB TABLE
    }
    if(i==MAXPROCESS){
        printf("ADD JOB ERROR\n");
        return;
    }//ADD ERROR
}

void Print_job(bgCon* data)
{
    qsort(bgCons, MAXPROCESS, sizeof(bgCons[0]), compare);
    for(int i=0; i<MAXPROCESS; i++){
        if(i>0)
            if(data[i-1].job_idx == data[i].job_idx)
                continue;//NEVER PRINT SAME JOB
        switch (data[i].bgSt)
        {
        case 0:
            printf("[%d] ", data[i].job_idx);
            printf("suspended ");
            printf("%s", data[i].bgCmd);
            break;//SUSPENDED
        
        case 1:
            printf("[%d] ", data[i].job_idx);
            printf("running ");
            printf("%s", data[i].bgCmd);
            break;//RUNNING
            
        default:
            break;//NOTHING
        }
    }
    return;
}

void JobStatus_empty(bgCon* data, int job_idx)
{
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            data[i].bgSt = -1;
            data[i].job_idx = -1;
            currNum--;
        }//CLEAR JOB TABLE
    }
}

void Wait_job(bgCon* data, int job_idx)
{
    int status;
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            Kill(data[i].bgPid, SIGCONT);
            Waitpid(data[i].bgPid, &status, WUNTRACED);
        }
    }//WAIT JOB
}

void JobStatus_stop(bgCon* data, int job_idx)
{
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            data[i].bgSt = 0;
        }
    }//CHANGE JOB TABLE INTO STOP
}

void JobStatus_run(bgCon* data, int job_idx)
{
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            data[i].bgSt = 1;
        }
    }//CHANGE JOB TABLE INTO RUN
}

void Run_job(bgCon* data, int job_idx)
{
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            Kill(data[i].bgPid, SIGCONT);
        }
    }//RUN JOB
}

void Kill_job(bgCon* data, int job_idx)
{
    for(int i=0; i<MAXPROCESS; i++){
        if(data[i].job_idx == job_idx){
            Kill(data[i].bgPid, SIGKILL);
        }
    }//KILL JOB
}