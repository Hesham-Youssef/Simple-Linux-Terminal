#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/resource.h>
#include <time.h>

FILE *f;
struct tm tm;
time_t t;

char* replaceWord(const char* s, const char* oldW, const char* newW) 
{ 
    char* result; 
    int i, cnt = 0; 
    int newWlen = strlen(newW); 
    int oldWlen = strlen(oldW); 
    for (i = 0; s[i] != '\0'; i++) { 
        if (strstr(&s[i], oldW) == &s[i]) { 
            cnt++; 
            i += oldWlen - 1; 
        } 
    }
    result = calloc(i + cnt * (newWlen - oldWlen) + 1, sizeof(char*)); 
  
    i = 0; 
    while (*s) { 
        if (strstr(s, oldW) == s) { 
            strcpy(&result[i], newW); 
            i += newWlen; 
            s += oldWlen; 
        } 
        else
            result[i++] = *s++; 
    } 
  
    result[i] = '\0'; 
    return result; 
} 

char* replaceEnvVars(char* str){
    char* key = NULL;
    char* val = NULL;
    int started = -1, len=strlen(str), end = -1;
    for(int i=0;i<len;i++){
        if(str[i] == '$'){
            started = i;
        }else if(started!=-1  && (!((str[i]>='a' && str[i]<='z') || (str[i]>='A' && str[i]<='Z')) || (i==(len-1)))){
            key = calloc(i-started+1, sizeof(char));
            strncpy(key,str+started, i-started);
            if(getenv(key+1) ==  NULL)
                str = replaceWord(str, key, "");
            else
                str = replaceWord(str, key, getenv(key+1));
            started = -1;
        }
    }
    return str;
}


void removeQuates(char* str){
    int i, j;
    int len = strlen(str);
    for(i=0; i<len; i++)
    {
        if(str[i] == '"')
        {
            for(j=i; j<len; j++)
            {
                str[j] = str[j+1];
            }
            len--;
            i--;
        }
    }
}


char** split_input(char* str){
  char ** res  = NULL;
  char *  p    = strtok (str, " ");
  int n_spaces = 0, i;


  while (p) {
      res = realloc (res, sizeof (char*) * ++n_spaces);
      memset(res[n_spaces-1], 0, sizeof(res[n_spaces-1]));
      if (res == NULL)
        exit (-1);
      res[n_spaces-1] = p;
      p = strtok (NULL, " ");
    }

  res = realloc (res, sizeof (char*) * (n_spaces+1));
  res[n_spaces] = 0;

  return res;
}

char* parse_input(char* str, int len){
    for (int i = len-1; i > -1; i--)
    {
        if(str[i] == 10){
            str[i] = ' ';
            break;
        }
    }
    str = replaceEnvVars(str);
}



void execute_shell_bultin(char* command){
    command = strtok(command, " ");
    if(strcmp("cd", command) == 0){
        command = strtok (NULL, " ");
        if((command == 0) || (command[0] == '~')){
            chdir(getenv("HOME"));
        }else if(chdir(command) != 0){
            perror("path isn't correct");
        }


    }else if(strcmp("echo", command) == 0){

        command = strtok(NULL, "\"");
        if(command == 0)
            printf("\n");
        else
            printf("%s\n", command);

    }else if(strcmp("export", command) == 0){

        command = strtok (NULL, "");
        if((command == 0)){
            perror("Command isn't complete");
        }else{
            removeQuates(command);
            char* key = strtok(command, "=");
            char* val = strtok(NULL, "");
            setenv(key, val, 1);

        }
    }
}


void execute_command(char* command, int len){
    pid_t child_id = fork();
    char** res = split_input(command);
    bool background = false;
    int i=0;
    while(res[i] != NULL){
        if(strncmp(res[i], "&", 1) == 0){
            background = true;
            res[i] = 0;
        }
        i++;
    }
    if(child_id == 0){
        execvp(res[0], res);
        printf("Error; command doesn't exist\n");
        exit(-1);
    }else{
        if(!background){
            wait(NULL);
            t = time(NULL);
            tm = *localtime(&t);
            fprintf(f, "%d-%02d-%02d %02d:%02d:%02d -- Child return code (foreground): %d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, child_id);
        }
    }
    memset(res, 0, sizeof(res));
}




int input_type(char* ins){
    if(strncmp("exit", ins, 4)==0)
        return 0;

    if(strncmp("cd", ins, 2)==0 || strncmp("echo", ins, 4)==0 || strncmp("export", ins, 6)==0)
        return 1;
    
    return 2;
}


void shell(){
    char *command = NULL;
    size_t len = 0;
    ssize_t lineSize = 0;
    char** parsed_input;
    char cwd[256];
    
    while(1){
        printf("%s: ", getcwd(cwd, sizeof(cwd)));
        command = NULL;
        lineSize = getline(&command, &len, stdin);

        command = parse_input(command, len);

        switch (input_type(command)){
            case 1:
                execute_shell_bultin(command);
                break;
            case 2:
                execute_command(command, len);
                break;
            
            case 0:
                free(command);
                return;
        }
        memset(command, 0, sizeof(command));


    }
    
}


void on_child_exit(){
    pid_t  pid;
    int stat;
    pid = waitpid(-1, &stat, WNOHANG);
    if(pid == 0 || pid == -1)
        return;
    else{
        t = time(NULL);
        tm = *localtime(&t);
        fprintf(f, "%d-%02d-%02d %02d:%02d:%02d -- Child return code(background): %d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, pid);
    }

}

void setup_environment(){
    chdir(getenv("HOME"));
    f = fopen("log.txt", "a+");
    if(f == 0){
        printf("error while creating log file");
        exit(-1);
    }
    fprintf(f, "\n----------------------NEW SESSION----------------------\n");
}

int main(int argc, char const *argv[]) {
    
    signal(SIGCHLD, on_child_exit);
    setup_environment();
    shell();

    return 0;
} 