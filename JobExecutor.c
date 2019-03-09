#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/time.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include "Trie.h"

#define READ 0
#define WRITE 1

#define MaxWords 10
#define MaxSize 1024
#define MaxLength 128

int main(int argc, char *argv[]){
    char* docfile;
    int numWorkers;
    int catalogs = 0;

    if(argv[1] == NULL){
        printf("Forgot to put options\n");
        return -1;
    }
    if(strcmp(argv[1], "-d") == 0){                                             //case where -d is first
        if(argv[2] != NULL){
            docfile = malloc(strlen(argv[2])*sizeof(char));
            strcpy(docfile, argv[2]);
        }else{
            printf("No docfile given\n");
            return -1;
        }
        if(strcmp(argv[3], "-w") == 0){
            if(argv[4] != NULL){
                numWorkers = atoi(argv[4]);
            }else{
                printf("numWorkers not given\n");
            }
        }
    }else if (strcmp(argv[1], "-w") == 0){                                      //case where -w is first
        if(argv[2] != NULL){
            numWorkers = atoi(argv[2]);
        }else{
            printf("numWorkers not given\n");
        }
        if(strcmp(argv[3], "-d") == 0){
            if(argv[4] != NULL){
                docfile = malloc(strlen(argv[2])*sizeof(char));
                strcpy(docfile, argv[2]);
            }else{
                printf("No docfile given\n");
                return -1;
            }
        }else{
            printf("Undefined options\n");
            return -1;
        }
    }else{                                                                      //default case
        printf("Undefined options\n");
        return -1;
    }

    FILE* fp = fopen(docfile, "r");
    if(fp < 0){
        perror("fopen");
        printf("Docfile given doesn't exist\n");
        return 0;
    }

    int bytesread;
    size_t n;
    while(1){                                                                   //loop counting the lines/catalog of docfile
        char* tempbuff = NULL;
        if((bytesread = getline(&tempbuff, &n, fp)) == -1){
            free(tempbuff);
            break;
        }
        if(bytesread > 1)                                                       //in case of empty lines(not considered as path)
            catalogs++;
        free(tempbuff);
    }

    if(catalogs < numWorkers){
        printf("Workers were more than the catalogs so they were reduced to %d\n", catalogs);
        numWorkers = catalogs;
    }
    pid_t pid, parent_pid;
    pid_t Workers[numWorkers];
    int fd[numWorkers][2];                                                      //2 = 1 for read and 1 for write
    char* pipe_name_in = (char*)malloc(5*sizeof(pid_t) + 3*sizeof(char));
    char* pipe_name_out = (char*)malloc(5*sizeof(pid_t) + 4*sizeof(char));
    parent_pid = getpid();
    int fd_Worker[2];
    int status = 0;
    for(int i = 0; i < numWorkers; i++){
        switch(pid = fork()){                                                   //simultaneously child and parent create pipes for reading/writing
            case -1:    perror("fork call"); exit(0);
            case  0:    kill(getpid(), SIGSTOP);                                //if child wait for parent to create the pipe
                        sprintf(pipe_name_in, "%d", getpid());
                        sprintf(pipe_name_out, "%d", getpid());
                        strcat(pipe_name_in, "_in");
                        strcat(pipe_name_out, "_out");
                        fd_Worker[WRITE] = open(pipe_name_out, O_WRONLY);       //only for writing
                        fd_Worker[READ] = open(pipe_name_in, O_RDONLY | O_NONBLOCK);//only for reading, it is NONBLOCK because I know when to terminate my input
                        break;
            default:    Workers[i] = pid;                                       //if parent store the pid
                        sprintf(pipe_name_in, "%d", pid);
                        sprintf(pipe_name_out, "%d", pid);
                        strcat(pipe_name_in, "_in");
                        strcat(pipe_name_out, "_out");
                        mkfifo(pipe_name_in, 0666);
                        mkfifo(pipe_name_out, 0666);
                        waitpid(pid, &status, WUNTRACED);                       //wait for child to be created and stopped
                        kill(pid, SIGCONT);                                     //when child stopped, make sure to restart it
                        fd[i][READ] = open(pipe_name_out, O_RDONLY);            //only for reading
                        fd[i][WRITE] = open(pipe_name_in, O_WRONLY);            //only for writing
        }
        if(pid == 0)    break;
    }

    srand(time(NULL));                                                          //initializing time
    int num;
    int num_of_bytes = 0;
    int num_of_words = 0;
    int num_of_lines = 0;
    int num_of_files = 0;
    int num_of_catalogs = catalogs/numWorkers;                                  //number of catalogs for every worker (maybe + 1 if remaining catalogs > 0)
    int remaining_catalogs = catalogs%numWorkers;                               //static
    int temp_remaining_catalogs = remaining_catalogs;                           //changing for the iterations
    int* doclines;                                                              //necessary for search and wc ( I keep number of lines of every file)
    char* buffer = malloc(MaxSize*sizeof(char));
    char** catalog = malloc((num_of_catalogs + (remaining_catalogs != 0))*sizeof(char*));
    if(getpid() == parent_pid){                                                 //passing the catalogs
        system("mkdir log");                                                    //creating folder for logs with system call
        fseek(fp, 0, SEEK_SET);                                                 //taking fp to the begining in order to read it again
        for(int i = 0; i < numWorkers; i++){
            waitpid(Workers[i], &status, WUNTRACED);
            for(int j = 0; j < (num_of_catalogs + (temp_remaining_catalogs != 0)); j++){//Iteration where temp_remaining_catalogs is changing dynamically, so every worker
                strcpy(buffer, "");                                             //takes the appropriate catalogs
                if((bytesread = getline(&buffer, &n, fp)) <= 1){
                    perror("getline");
                    return -1;
                }
                buffer[strlen(buffer) - 1] = '\0';                              //replacing \n with \0
                if(buffer[0] == '.'){                                           //I use relative paths so place dot in the beggining
                    write(fd[i][WRITE], buffer, MaxSize);
                }else{
                    char* tempb = malloc(strlen(buffer + 1)*sizeof(char));
                    strcpy(tempb,".");
                    strcat(tempb, buffer);
                    write(fd[i][WRITE], tempb, MaxSize);
                }
                kill(Workers[i], SIGCONT);                                      //continue child now that parent wrote in pipe
                waitpid(Workers[i], &status, WUNTRACED);                        //parent waits for child to read what is written in pipe
            }
            if(temp_remaining_catalogs > 0)  temp_remaining_catalogs--;
            strcpy(buffer, "");
            kill(Workers[i], SIGCONT);
        }
    }else{                                                                      //worker part
        num = 0;
        kill(getpid(),SIGSTOP);                                                 //wait for parent to resume child and go read from pipe
        while((bytesread = read(fd_Worker[READ], buffer, MaxSize) > 0)){
            catalog[num] = malloc(strlen(buffer)*sizeof(char));
            strcpy(catalog[num],"");
            strcpy(catalog[num], buffer);
            num++;
            kill(getpid(), SIGSTOP);
        }
        if((num == num_of_catalogs) && (remaining_catalogs > 0)){               //I declare an array with an extra more space for all workers
            catalog[num] = NULL;                                                //ex. 4 workers with 5 catalogs, first worker takes 2files and every other gets one
        }                                                                       //catalog array is 2 for all of them because they do not know that from the begining

        char ch;
        int docs[num];
        for(int i = 0; i < num; i++){
            docs[i] = 0;
        }
        int count = 0;

        DIR *dir;
        struct dirent *ent;
        char ***FileNames = malloc(num*sizeof(char**));                         //array for storing all names of every file inside every catalog
        for(int i = 0; i < num; i++){
            if(catalog[i] != NULL){
                if((dir = opendir(catalog[i])) != NULL){
                    while((ent = readdir(dir)) != NULL){                        //searching directory for number of files
                        if((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) docs[i]++;
                    }
                }else{
                    perror("opendir");
                    return -1;
                }
                rewinddir(dir);                                                 //rewinding directory to search it again, now for names
                FileNames[i] = malloc(docs[i]*sizeof(char*));
                count = 0;
                while((ent = readdir(dir)) != NULL){
                    if((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)){
                        strcpy(buffer, ent->d_name);
                        FileNames[i][count] = malloc(strlen(buffer)*sizeof(char));
                        strcpy(FileNames[i][count], buffer);
                        count++;
                    }
                }
                closedir(dir);
            }
        }

        char* filename;
        CreateTrie();                                                           //Trie init

        int j = 0;
        num_of_files = 0;
        for(int i = 0; i < num; i++){
            num_of_files += docs[num];
        }
        doclines = malloc(num_of_files*sizeof(int));
        num_of_files = 0;
        for(int i = 0; i < num; i++){
            for(int k = 0; k < docs[i]; k++){
                if(catalog[i] != NULL){
                    filename = malloc((strlen(FileNames[i][k]) + strlen(catalog[i]))*sizeof(char));
                    strcpy(filename, catalog[i]);
                    strcat(filename, "/");
                    strcat(filename, FileNames[i][k]);                          //making the full relative path for file to fopen
                    //printf("filename = %.50s\n",filename);
                    FILE* file = fopen(filename, "r");
                    if(file == NULL){
                        perror("fopen");
                        return -1;
                    }
                    num_of_files++;

                    while(1){                                                   //read all file character by character and insert into map
                       ch = fgetc(file);
                       if(feof(file)){
                           break;
                       }else{
                           num_of_bytes++;
                           if(ch == '\n'){
                               Insert(' ', j, num_of_files - 1, filename);      //insert space to change the word (pointer in insert)
                               j++;
                               num_of_lines++;
                           }else{
                               if(CheckValidity(ch) == 0)   num_of_words++;
                               Insert(ch, j, num_of_files - 1, filename);
                           }
                       }
                   }
                   free(filename);
               }
               doclines[num_of_files - 1] = j;                                  //array where storing how many lines there are on each file
               j = 0;
           }
       }                                                                        //Trie is now ready
       char* word = malloc(MaxSize*sizeof(char));
       char* pidd = malloc(10*sizeof(char));
       sprintf(pidd, "%d", getpid());
       strcat(pidd,".txt");
       //FILE *fp3 = fopen(pidd, "w+");
       //df(GetRoot(), word, 0, fp3);
       //fclose(fp3);
       free(word);
       free(pidd);
       for(int k = 0; k < num; k++){
           for(int l = 0; l < docs[k]; l++){
               free(FileNames[k][l]);
           }
           free(FileNames[k]);
       }
       free(FileNames);
    }

    if(getpid() == parent_pid){
        char* word = malloc((MaxSize + 1)*sizeof(char));
        strcpy(word,"");
        char* InputLine = malloc((MaxWords + 2)*MaxSize*sizeof(char));          //buffer for 10 words * Max Length
        char* Query = malloc(10*MaxSize*sizeof(char));
        printf("Waiting command...\n");
        if(fgets(InputLine, 10*MaxSize, stdin) == NULL){
            perror("Error, no argument given\n");                               //reading the "arguments"
        }
        strcpy(Query, InputLine);                                               //keeping a clean copy of input line to send to workers because strtok affects inputline
        char** arguments = malloc((MaxWords + 3)*sizeof(char*));
        char* result = malloc(MaxSize*sizeof(char));
        arguments[0] = strtok(InputLine, " \n");                                //using strtok function for delimiting words with space or \n
        if(arguments[0] == NULL)    arguments[0] = word;
        while(strcmp(arguments[0], "/exit") != 0){

            if(strcmp(arguments[0], "/maxcount") == 0){
                arguments[1] = strtok(NULL, " \n");
                if(arguments[1] == NULL){                                       //keyword exists
                    printf("Wrong Format! [/maxcount keyword]\n");
                }else{
                    int times;
                    int max = 0;
                    char* maxpath = malloc(MaxSize*sizeof(char));
                    char* path;
                    char* stimes;
                    for(int i = 0; i < numWorkers; i++){                        //writing to a pipe and waking up the worker
                        write(fd[i][WRITE], Query, MaxSize);
                        waitpid(Workers[i], &status, WUNTRACED);
                        kill(Workers[i], SIGCONT);
                    }
                    for(int i = 0; i < numWorkers; i++){                        //waiting for answers. Used linear approach on the subject instead of poll()
                        read(fd[i][READ], result, MaxSize);                     //father blocks on read waiting for answer.
                        path = strtok(result,"\n");
                        if(strcmp(path,"0") != 0){
                            stimes = strtok(NULL, "\0");
                            times = atoi(stimes);
                            if(times > max){
                                max = times;
                                strcpy(maxpath, path);
                            }else if(times == max){
                                if(strcmp(maxpath,path) > 0)    strcpy(maxpath,path);
                            }
                        }
                        strcpy(result, "");
                    }
                    if(max != 0){                                               //max = 0 if the word was not found anywhere
                        printf("Path: %.50s with %d times.\n", maxpath, max);
                    }else{
                        printf("This keyword was not found in any file.\n");
                    }
                    free(maxpath);
                }
            }else if(strcmp(arguments[0], "/mincount") == 0){
                arguments[1] = strtok(NULL, " \n");
                if(arguments[1] == NULL){                                       //keyword exists
                    printf("Wrong Format! [/mincount keyword]\n");
                }else{
                    int times;
                    int min = INT_MAX - 1;
                    char* minpath = malloc(MaxSize*sizeof(char));
                    char* path;
                    char* stimes;
                    for(int i = 0; i < numWorkers; i++){
                        write(fd[i][WRITE], Query, MaxSize);
                        waitpid(Workers[i], &status, WUNTRACED);
                        kill(Workers[i], SIGCONT);
                    }
                    for(int i = 0; i < numWorkers; i++){
                        read(fd[i][READ], result, MaxSize);
                        path = strtok(result,"\n");
                        if(strcmp(path, "0") != 0){
                            stimes = strtok(NULL, "\0");
                            times = atoi(stimes);
                            if(times < min){
                                min = times;
                                strcpy(minpath, path);
                            }else if(times == min){
                                if(strcmp(minpath,path) > 0)    strcpy(minpath,path);
                            }
                        }
                        strcpy(result, "");
                    }
                    if(min != INT_MAX - 1){
                        printf("Path: %.50s with %d times.\n", minpath, min);
                    }else{
                        printf("This keyword was not found in any file.\n");
                    }
                    free(minpath);
                }
            }else if(strcmp(arguments[0], "/search") == 0){
                arguments[1] = strtok(NULL, " \n");
                if((arguments[1] == NULL) || (strcmp(arguments[1],"-d") == 0)){
                    printf("Unknown command! [You have to type queries also.]\n");
                }else{
                    int i;
                    float DeadLine;
                    int correct_format = 0;
                    for(i = 2; i <= 12; i++){
                        arguments[i] = strtok(NULL, " \n");
                        if(arguments[i] == NULL) break;
                        if((strcmp(arguments[i],"-d") == 0) && (i < 12)){
                            correct_format = 1;
                            arguments[i + 1] = strtok(NULL, " \n");
                            DeadLine = atof(arguments[i + 1]);
                            break;
                        }
                    }
                    if(correct_format == 0){                                    //if more than 10 words were given in input
                        if(i == 12){
                            if(strcmp(arguments[12], "-d") != 0){
                                while(strcmp(arguments[11], "-d") != 0){
                                    arguments[11] = strtok(NULL, " \n");
                                }
                                arguments[12] = strtok(NULL, " \n");
                                DeadLine = atof(arguments[12]);
                            }else{
                                arguments[11] = arguments[12];
                                arguments[12] = strtok(NULL, " \n");
                                DeadLine = atof(arguments[12]);
                            }
                            correct_format = 1;
                        }else{
                            printf("Wrong format! Entering a deadline with -d is mandatory.\n");
                        }
                    }
                    if(correct_format == 1){                                    //format is now corrected so parent sends to wroker the query
                        i = 0;
                        strcpy(Query,"");
                        while((arguments[i] != NULL) && (i <= 12)){             //reconstructing the query with the first arguments
                            if((strlen(Query) + strlen(arguments[i])) < MaxSize){
                                strcat(Query, arguments[i]);
                                strcat(Query, " ");
                            }
                            i++;
                        }
                        for(i = 0; i < numWorkers; i++){
                            write(fd[i][WRITE], Query, MaxSize);
                            waitpid(Workers[i], &status, WUNTRACED);
                            kill(Workers[i], SIGCONT);
                        }
                        char* FullPath;
                        char* sdeadline = malloc(MaxSize*sizeof(char));
                        int num_of_answers = numWorkers;                        //keeping track of how many workers answered
                        float deadline[numWorkers];
                        size_t bytesread = 0;
                        size_t temp;
                        char** Answers = malloc(numWorkers*sizeof(char*));
                        for(int i = 0; i < numWorkers; i++){
                            temp = read(fd[i][READ], sdeadline, MaxSize);       //reading deadline
                            deadline[i] = atof(sdeadline);
                            if(deadline[i] > DeadLine)  num_of_answers--;
                            waitpid(Workers[i], &status, WUNTRACED);
                            kill(Workers[i], SIGCONT);

                            temp = read(fd[i][READ], result, MaxSize);
                            Answers[i] = malloc(0);                             //initialize o null pointer in order for realloc to work
                            while(strcmp(result, "/eow") != 0){                 //waiting for eow = end of worker to stop reading from his pipe
                                bytesread += temp;                              //I dont double it because since the pipe is very big not many reallocs will be needed
                                Answers[i] = realloc(Answers[i],(bytesread * sizeof(char)));//if the answer is exceeding the max size I realloc it to the new size
                                if(bytesread == temp)   strcpy(Answers[i],"");  //first realloc = malloc
                                strcat(Answers[i], result);
                                waitpid(Workers[i], &status, WUNTRACED);
                                kill(Workers[i], SIGCONT);
                                strcpy(result, "");
                                temp = read(fd[i][READ], result, MaxSize);
                            }
                            bytesread = 0;
                        }
                        printf("\n%d of %d workers answered\n",num_of_answers, numWorkers);
                        for(int i = 0; i < numWorkers; i++){
                            if(deadline[i] < DeadLine)                          //if the worker answered before deadline
                                printf("\nResults by %d:\n%s\n",Workers[i], Answers[i]);
                            strcpy(Answers[i],"");
                            free(Answers[i]);
                        }
                        free(Answers);
                        free(sdeadline);
                    }
                }
                for(int i = 0; i <= 12; i++){
                    arguments[i] = NULL;
                }
                strcpy(Query,"");
            }else if(strcmp(arguments[0], "/wc") == 0){
                for(int i = 0; i < numWorkers; i++){
                    write(fd[i][WRITE], Query, MaxSize);
                    waitpid(Workers[i], &status, WUNTRACED);
                    kill(Workers[i], SIGCONT);
                }
                char* sbytes;
                char* swords;
                char* slines;
                int bytesum = 0;
                int wordsum = 0;
                int linesum = 0;
                for(int i = 0; i < numWorkers; i++){
                    read(fd[i][READ], result, MaxSize);
                    sbytes = strtok(result, "\n");
                    bytesum += atoi(sbytes);
                    swords = strtok(NULL, "\n");
                    wordsum += atoi(swords);
                    slines = strtok(NULL, " \n");
                    linesum += atoi(slines);
                }
                printf("Job Executor: Total %d bytes, %d words and %d lines\n", bytesum, wordsum, linesum);
            }else{
                printf("Unknown command\n");
            }
            strcpy(InputLine, "");                                              //emptying my buffer
            printf("Waiting command...\n");
            if(fgets(InputLine, 10*MaxSize, stdin) == NULL){
                printf("Error, no argument given\n");
            }
            strcpy(Query, InputLine);
            arguments[0] = strtok(InputLine, " \n");
            if(arguments[0] == NULL)    arguments[0] = word;                    //default value if no entry given
        }
        for(int i = 0; i < numWorkers; i++){
            write(fd[i][WRITE], "/exit", MaxSize);
            waitpid(Workers[i], &status, WUNTRACED);
            kill(Workers[i], SIGCONT);
        }
        free(word);
        free(result);
        free(Query);
        free(InputLine);
        free(arguments);
    }else{
        kill(getpid(),SIGSTOP);
        FILE* logfile;
        time_t t = time(NULL);
        struct tm tm;
        char* logname = malloc(11*sizeof(char));
        char* cid = malloc(4*sizeof(char));
        strcpy(logname, "./log/Worker_");
        sprintf(cid, "%d", getpid());
        strcat(logname, cid);
        logfile = fopen(logname, "w+");
        free(cid);
        free(logname);
        char** arguments = malloc((MaxWords + 3)*sizeof(char*));
        while((bytesread = read(fd_Worker[READ], buffer, MaxSize) > 0)){        //job executor has cheked for correct format
            t = time(NULL);
            tm = *localtime(&t);
            arguments[0] = strtok(buffer, " \n");                               //using strtok function for delimiting words with space or \n
            if(strcmp(arguments[0],"/exit") == 0) break;
            if(strcmp(arguments[0], "/maxcount") == 0){
                char* timestamp = malloc(MaxLength*sizeof(char));
                sprintf(timestamp, "%d-%d-%d %d %d %d: maxcount ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                arguments[1] = strtok(NULL, " \n");
                PListNode* max = maxcount(arguments[1]);
                char* amount = malloc(20*sizeof(char));
                if(max != NULL){
                    fprintf(logfile, "%s", timestamp);                          //writing to log file
                    fprintf(logfile, " %s : ", arguments[1]);
                    strcpy(buffer, max->DocName);
                    fprintf(logfile, " %s\n", max->DocName);
                    strcat(buffer, "\n");
                    sprintf(amount, "%d", max->Amount);
                    strcat(buffer, amount);
                    strcat(buffer, "\n");
                    write(fd_Worker[WRITE], buffer, MaxSize);                   //sending answer to job executor
                }else{                                                          //not found
                    strcpy(buffer, "0");
                    strcat(buffer, "\n");
                    write(fd_Worker[WRITE], buffer, MaxSize);
                }
                free(amount);
                free(timestamp);
            }else if(strcmp(arguments[0], "/mincount") == 0){
                char* timestamp = malloc(MaxLength*sizeof(char));
                sprintf(timestamp, "%d-%d-%d %d %d %d: mincount ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                arguments[1] = strtok(NULL, " \n");
                PListNode* min = mincount(arguments[1]);
                char* amount = malloc(20*sizeof(char));
                if(min != NULL){
                    fprintf(logfile, "%s", timestamp);                          //writing to log file
                    fprintf(logfile, " %s : ", arguments[1]);
                    strcpy(buffer, min->DocName);
                    fprintf(logfile, "%s\n", min->DocName);
                    strcat(buffer, "\n");
                    sprintf(amount, "%d", min->Amount);
                    strcat(buffer, amount);
                    strcat(buffer, "\n");
                    write(fd_Worker[WRITE], buffer, MaxSize);                   //writing the answer to job executor
                }else{
                    strcpy(buffer, "0");                                        //not found
                    strcat(buffer, "\n");
                    write(fd_Worker[WRITE], buffer, MaxSize);
                }
                free(amount);
                free(timestamp);
            }else if(strcmp(arguments[0], "/search") == 0){
                double starting_time = my_clock();
                char* timestamp = malloc(MaxLength*sizeof(char));
                sprintf(timestamp, "%d-%d-%d %d %d %d: search :", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                double end_time;
                char** Words = malloc(10*sizeof(char*));                        //array for worker words
                int flag = 1;
                float deadline;
                int** Sent = malloc(num_of_files*sizeof(int*));                 //Array for evey file and every line in order to flag those who I want to print
                char** FullPath = malloc(num_of_files*sizeof(char*));           //Array for storing full paths to those files that some lines needs to be printed
                for(int i = 0; i < num_of_files; i++){
                    FullPath[i] = NULL;
                }
                for(int i = 0; i < num_of_files; i++){                          //Initializing array(sent = to know what lines are to be printed and whether a line is already flaged)
                    Sent[i] = malloc(doclines[i]*sizeof(int));
                    for(int k = 0; k < doclines[i]; k++){
                        Sent[i][k] = 0;
                    }
                }
                for(int i = 0; i < 10; i++){                                    //Initializing words
                    Words[i] = strtok(NULL, " \n");
                    if(strcmp(Words[i],"-d") == 0){
                        arguments[1] = Words[i];
                        Words[i] = NULL;
                        arguments[2] = strtok(NULL, " \n");
                        flag = 0;
                        break;
                    }
                }

                if(flag == 1){
                    arguments[1] = strtok(NULL, " \n");
                    arguments[2] = strtok(NULL, " \n");
                }
                deadline = atof(arguments[2]);                                  //converting deadline from string to double
                int i = 0;
                while((Words[i] != NULL) && (i < 10)){
                    PListNode* PLNode = dfSingle(Words[i], 0);                  //return the posting list of the word searched
                    i++;
                    fprintf(logfile, "%s %s ", timestamp, Words[i - 1]);        //writing on log file the timestamp
                    while(PLNode != NULL){                                      //if it exists and not null, worker will flag all the positions in sent array to know who to print
                        if(Sent[PLNode->File_id][PLNode->Doc_id] == 0){
                            Sent[PLNode->File_id][PLNode->Doc_id] = 1;          //flaggin the line on that file
                            if(FullPath[PLNode->File_id] == NULL)
                                FullPath[PLNode->File_id] = malloc(strlen(PLNode->DocName)*sizeof(char));
                            strcpy(FullPath[PLNode->File_id], PLNode->DocName);
                        }
                        fprintf(logfile, ": %s ", PLNode->DocName);             //writing on log
                        PLNode = PLNode->Next;
                    }
                    fprintf(logfile, "\n");
                }

                char* tempbuff = NULL;
                size_t n;
                size_t ArrSize = 1;
                char* buff = malloc(MaxSize*sizeof(char));
                char** Answers = malloc(ArrSize*sizeof(char*));                 //buffer with all the answers to job executor
                Answers[0] = malloc(MaxSize*sizeof(char));                      //starting from a small size and then reallocing to a bigger size if the output is big
                strcpy(Answers[0], "");
                int line = 0;
                char* message1 = malloc(20*sizeof(char));                       //default messages for the answers on job executor
                strcpy(message1,"Path: ");
                char* message2 = malloc(20*sizeof(char));
                strcpy(message2,"#Line: ");
                char* message3 = malloc(20*sizeof(char));
                strcpy(message3,"Contents: ");
                flag = 0;
                for(int i = 0; i < num_of_files; i++){                          //for every file open it and check for every line if it is flagged by Sent array
                    if(FullPath[i] != NULL){
                        FILE* file = fopen(FullPath[i], "r");
                        if(file == NULL){
                            perror("fopen");
                            return -1;
                        }
                        line = 0;
                        char* sline = malloc(10*sizeof(char));
                        while(((bytesread = getline(&tempbuff, &n, file)) > 1)){
                            if(Sent[i][line] == 1){                             //if it is flagged I include it in the answer.
                                flag = 1;
                                if((strlen(FullPath[i]) + strlen(message1) + 1) < MaxSize - strlen(Answers[ArrSize - 1])){//if path doesnt fit
                                    strcat(Answers[ArrSize - 1], message1);
                                    strcat(Answers[ArrSize - 1], FullPath[i]);
                                    strcat(Answers[ArrSize - 1], "\n");
                                }else{
                                    Answers = realloc(Answers, (++ArrSize)*sizeof(char*));
                                    Answers[ArrSize - 1] = malloc(MaxSize*sizeof(char));
                                    strcpy(Answers[ArrSize - 1], message1);
                                    strcat(Answers[ArrSize - 1], FullPath[i]);
                                    strcat(Answers[ArrSize - 1], "\n");
                                }
                                sprintf(sline , "%d", line);
                                if((strlen(sline) + strlen(message2) + 1) < MaxSize - strlen(Answers[ArrSize - 1])){//if line number doesnt fit
                                    strcat(Answers[ArrSize - 1], message2);
                                    strcat(Answers[ArrSize - 1], sline);
                                    strcat(Answers[ArrSize - 1], "\n");
                                }else{
                                    Answers = realloc(Answers, (++ArrSize)*sizeof(char*));
                                    Answers[ArrSize - 1] = malloc(MaxSize*sizeof(char));
                                    strcpy(Answers[ArrSize - 1], message2);
                                    strcat(Answers[ArrSize - 1], sline);
                                    strcat(Answers[ArrSize - 1], "\n");
                                }
                                if((strlen(tempbuff) + strlen(message3)) < MaxSize - strlen(Answers[ArrSize - 1])){//if buffer with line doesnt fit
                                    strcat(Answers[ArrSize - 1], message3);
                                    strcat(Answers[ArrSize - 1], tempbuff);
                                }else{
                                    Answers = realloc(Answers, (++ArrSize)*sizeof(char*));
                                    Answers[ArrSize - 1] = malloc(MaxSize*sizeof(char));
                                    if((strlen(tempbuff) + strlen(message3)) < MaxSize){
                                        strcpy(Answers[ArrSize - 1], message3);
                                        strcat(Answers[ArrSize - 1], tempbuff);
                                    }else{
                                        strcpy(Answers[ArrSize - 1], message3);
                                        strcat(Answers[ArrSize - 1], "Error! Line was too long for a preview.\n");
                                    }
                                }
                            }
                            line++;
                            //tempbuff = NULL;
                        }
                    }
                }
                if(flag == 0){
                    strcpy(Answers[ArrSize - 1], "Not found\n");
                }

                end_time = my_clock();                                          //stop timer
                double current_time = (end_time - starting_time);               //time to complete
                char* scurrent_time = malloc(MaxLength*sizeof(char));
                sprintf(scurrent_time, "%lf", current_time);
                write(fd_Worker[WRITE], scurrent_time, MaxSize);                //first item sent to parent is the timer
                kill(getpid(), SIGSTOP);
                free(scurrent_time);

                for(int k = 0; k < ArrSize; k++){
                    write(fd_Worker[WRITE], Answers[k], MaxSize);               //then all the answers
                    free(Answers[k]);
                    kill(getpid(), SIGSTOP);
                }
                write(fd_Worker[WRITE], "/eow", MaxSize);                       //send eow to parent to know that the answer is over
                free(timestamp);
                free(Answers);
                free(Words);
                for(int k = 0; k < num_of_files; k++){
                    free(Sent[k]);
                    free(FullPath[k]);
                }
                free(Sent);
                free(FullPath);
                free(message1);
                free(message2);
                free(message3);
                arguments[1] = NULL;
                arguments[2] = NULL;

            }else if(strcmp(arguments[0], "/wc") == 0){
                char* timestamp = malloc(MaxLength*sizeof(char));
                sprintf(timestamp, "%d-%d-%d %d %d %d: wc ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                //printf("Worker %u has %d bytes, %d words and %d lines totally.\n", getpid(), num_of_bytes, num_of_words, num_of_lines);
                fprintf(logfile, "%s : ", timestamp);
                fprintf(logfile, "#bytes : %d : #words : %d : #lines : %d\n", num_of_bytes, num_of_words, num_of_lines);
                char* bytes = malloc(15*sizeof(char));
                char* words = malloc(10*sizeof(char));
                char* slines = malloc(10*sizeof(char));
                sprintf(bytes, "%d", num_of_bytes);
                sprintf(words, "%d", num_of_words);
                sprintf(slines, "%d", num_of_lines);
                strcpy(buffer, bytes);
                strcat(buffer, "\n");
                strcat(buffer, words);
                strcat(buffer, "\n");
                strcat(buffer, slines);
                strcat(buffer, "\n");
                write(fd_Worker[WRITE], buffer, MaxSize);
                free(bytes);
                free(words);
                free(slines);
                free(timestamp);
            }
            strcpy(buffer,"");
            kill(getpid(), SIGSTOP);                                            //after every command workers are blocked here, and not in the read pipe because it is non block
        }                                                                       //and waiting for /exit to exit
        fclose(logfile);
        free(arguments);
    }

    if(getpid() == parent_pid){                                                 //parent exiting
        free(buffer);
        free(catalog);
        for(int k = 0; k < numWorkers; k++){
            waitpid(Workers[k], &status, WUNTRACED);
            sprintf(pipe_name_in, "%d", Workers[k]);
            strcat(pipe_name_in,"_in");
            if(unlink(pipe_name_in) < 0)    perror("unlink");
            sprintf(pipe_name_out, "%d", Workers[k]);
            strcat(pipe_name_out,"_out");
            if(unlink(pipe_name_out) < 0)    perror("unlink");
            close(fd[k][READ]);
            close(fd[k][WRITE]);

        }
        free(pipe_name_in);
        free(pipe_name_out);
    }else{                                                                      //child exiting
        for(int k = 0; k < num_of_catalogs + (remaining_catalogs != 0); k++){
            if(catalog != NULL)
                free(catalog[k]);
        }
        free(catalog);
        TrieDelete(GetRoot());
        close(fd_Worker[READ]);
        close(fd_Worker[WRITE]);
        printf("Exiting %u\n", getpid());
    }
    return 0;
}
