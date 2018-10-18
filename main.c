#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/wait.h>

#define HELP_INFORMATION       "daemonGenius [-?hvV] [-d debug] [-s stop] [-t value] [-p pidfile] [-l logfile] [-m path args...]"
#define DGENIUS_VER             "ver 1.0.2"
#define AUTHOR_NAME             "albertsong"

#define MAX_EXEC_ARGV_NUM   5

char *path = NULL;
char *exc_argv[MAX_EXEC_ARGV_NUM];

char restart = 0;
pid_t childPid = 0;
FILE *logFile = NULL;
char *pidFileNamePath="DGenius.pid";
char *logFileNamePath="DGenius.log";
int debugMode = 0;

char *dg_strdup(const char *s)
{
    if(s == NULL){
        return NULL;
    }
    int len = strlen(s);
    char *dst = (char *)malloc(len+1);
    if(dst){
        strncpy(dst,s,len+1);
    }
    return dst;
}
char logTitleBuffer[50];
char *logTile(void)
{
    time_t tp = time(NULL);
    assert(tp!=-1);
    struct tm localTm;
    localtime_r(&tp,&localTm);
    logTitleBuffer[0] = 0;
    logTitleBuffer[1] = 0;
    snprintf(logTitleBuffer,50,"%04d%02d%02d %02d:%02d:%02d: ",localTm.tm_year+1900,localTm.tm_mon+1,localTm.tm_mday,
            localTm.tm_hour,localTm.tm_min,localTm.tm_sec);
    return logTitleBuffer;
}
void launchProgram(char *path,char *exc_argv[])
{
    pid_t pid = fork();
    if(pid == -1){
        char buffer[20];
        fprintf(logFile,"%sfork error %s\n",logTile(),strerror_r(errno,buffer,20));
        if(!debugMode){
            fclose(logFile);
        }
        abort();
    }else if(pid == 0){
        //launch
        if(!debugMode){
            fclose(logFile);
        }
        execv(path,exc_argv);
        abort();
    }
    //continue
    childPid = pid;
}
static void signalHandlerForChildExit(int sig)
{
    int status = 0;
    pid_t pid = 0;
    int exitcode = 0;
    
    do{
        pid  = waitpid(-1,&status,WNOHANG);
        if(pid == -1){
            int err = errno;
            if(err == EINTR){
                continue;
            }
            if(err == ECHILD){
                errno = 0;
                break;
            }
            char buffer[20];
            fprintf(logFile,"%swait pid fail %s\n",logTile(),strerror_r(errno,buffer,20));
            break ;
        }else if(pid > 0){
            if(WIFEXITED(status)){
                exitcode = WEXITSTATUS(status);
            }else if(WIFSIGNALED(status)){
                exitcode = WTERMSIG(status);
            }else if(WIFSTOPPED(status)){
                exitcode = WSTOPSIG(status);
            }
        }else{
            break;
        }
        if(childPid == pid){
            restart = 1;
            fprintf(logFile,"%schild exited with status code %d\n",logTile(),exitcode);
        }
    }while(pid > 0);
}
int setPidFile(int pid)
{
    FILE *pidFile;
    if(pid == 0){
        pidFile = fopen(pidFileNamePath,"r");
        if(pidFile == NULL){
            printf("can not open %s\n",pidFileNamePath);
            return 0;
        }
        char buf[20];
        memset(buf,0,20);
        fread(buf,1,20,pidFile);
        //puts(buf);
        pid = atoi(buf);
    }else{
        pidFile = fopen(pidFileNamePath,"w+");
        if(pidFile == NULL){
            printf("can not open %s\n",pidFileNamePath);
            return 0;
        }
        fprintf(pidFile,"%d",pid);
    }
    fclose(pidFile);
    return pid;
}
void signalHandlerInitialize(void)
{
    struct sigaction sa_usr;
    memset(&sa_usr,0,sizeof(sa_usr));
    sa_usr.sa_handler = signalHandlerForChildExit;
    sigemptyset(&(sa_usr.sa_mask));
    sigaddset(&(sa_usr.sa_mask),SIGCHLD);
    if(sigaction(SIGCHLD,&sa_usr,NULL) == -1){
        perror("sigaction");
        abort();
    }
}
int main(int argc, char const *argv[])
{
    int i=0;
    int restartTimes = -1;
    if(argc < 2){
        puts(HELP_INFORMATION);
        exit(EXIT_FAILURE);
    }
    for(i=0;i<MAX_EXEC_ARGV_NUM;i++){
        exc_argv[i] = NULL;
    }
    for(i=1;i<argc;i++){
        if(argv[i][0] != '-'){
            printf("invalid parameter %s\n",argv[i]);
            exit(EXIT_FAILURE);
        }
        switch(argv[i][1]){
            case '?':
            case 'h':
                puts(HELP_INFORMATION);
                break;
            case 'v':
                puts(DGENIUS_VER);
                break;
            case 'V':
                printf("%s build %s %s all copyright at %s\n",DGENIUS_VER,__DATE__,__TIME__,AUTHOR_NAME);
                break;
            case 'm':
                path = dg_strdup(argv[++i]);
                if(path == NULL){
                    printf("the option -m require file path\n");
                    exit(EXIT_FAILURE);
                }
                int cnt=0;
                i++;
                exc_argv[cnt++] = dg_strdup(path);
                while(i<argc){
                    exc_argv[cnt++] = dg_strdup(argv[i++]); 
                    if(cnt > MAX_EXEC_ARGV_NUM){
                        printf("too many args for program\n");
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            case 's':
                if(NULL == argv[i+1]){
                    puts("-s require option stop");
                    exit(EXIT_FAILURE);
                }
                if(0==strcmp(argv[i+1],"stop")){
                    pid_t pid = setPidFile(0);
                    if(pid != 0){
                        if(0==kill(pid,SIGKILL)){
                            puts("stop successfull");
                        }else{
                            perror("stop");
                        }
                    }else{
                        puts("not ready or running");
                    }
                }else{
                    puts("-s require option stop");
                    exit(EXIT_FAILURE);
                }
                i++;
                break;
            case 't':
                if(NULL== argv[i+1]){
                    puts("-s require value restart times");
                    exit(EXIT_FAILURE);
                }
                restartTimes = atoi(argv[i+1]);
                i++;
                break;
            case 'd':
                if(NULL==argv[i+1]){
                    puts("-d require debug mode");
                    exit(EXIT_FAILURE);
                }
                if(0==strcmp(argv[i+1],"debug")){
                    debugMode = 1;
                }else{
                    puts("-d require debug mode");
                    exit(EXIT_FAILURE);
                }
                i++;
                break;
            case 'p':
                if(NULL== argv[i+1]){
                    puts("-p require pid file store path & name");
                    exit(EXIT_FAILURE);
                }
                pidFileNamePath = dg_strdup(argv[i+1]);
                i++;
                break;
            case 'l':
                if(NULL== argv[i+1]){
                    puts("-l require log file store path & name");
                    exit(EXIT_FAILURE);
                }
                logFileNamePath = dg_strdup(argv[i+1]);
                i++;
                break;
            default:
                printf("invalid parameter %s\n",argv[i]);
                exit(EXIT_FAILURE);
        }
    }
    if(path == NULL){
        //printf("you need to input monitored program file path with [-m path]\n");
        exit(EXIT_FAILURE);
    }
    for(i=0;i<MAX_EXEC_ARGV_NUM;i++){
        printf("%s ",exc_argv[i]);
    }
    puts("");

    signalHandlerInitialize();

    if(!debugMode){
        if(-1 == daemon(1,0)){
            perror("daemon");
            exit(EXIT_FAILURE);
        }
        logFile = fopen(logFileNamePath,"a+");
        if(logFile == NULL){
            abort();    //gdb debug
        }
        setbuf(logFile,NULL);
    }else{
        logFile = stdout;
    }
    setPidFile(getpid());

    sigset_t     set;
    sigemptyset(&set);
    sigaddset(&set,SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        fprintf(logFile,"%sDGenius sigproc mask fail\n",logTile());
        if(!debugMode){
            fclose(logFile);
        }
        abort();
    }

    //launchProgram(path,exc_argv);
    restart = 1;
    fprintf(logFile,"%sDGenius start ok\n",logTile());
    sigemptyset(&set);
    while(1 && restartTimes!=0){
        if(restart){
            restart = 0;
            fprintf(logFile,"%sDGenius restart program [%s]\n",logTile(),path);
            launchProgram(path,exc_argv);
            if(restartTimes > 0){
                restartTimes--;
            }
        }
        
        sigsuspend(&set);
    }
    if(restartTimes == 0){
        fprintf(logFile,"too many restart times\n");
    }
    if(!debugMode){
        fclose(logFile);
    }
    
    return 0;
}

