#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/wait.h>
#include "kvstore.h"

#define DATAFILE "data.db"

//Get size of database in bytes
void size_command(void)
{
    pid_t pid = fork();
    if (pid < 0) 
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0) 
    {
        //Child: replace this process with `wc -c data.db`
        execlp("wc", "wc", "-c", DATAFILE, (char *)NULL);
        //If execlp returns, exec failed
        perror("execlp");
        _exit(1);
    } 
    else 
    {
        //Parent: wait for the child to finish
        int status;
        if(waitpid(pid, &status, 0) < 0) 
        {
            perror("waitpid");
            exit(1);
        }
    }
}

//Set new key in database
void set(const char *key, const char *value)
{
    int fd = open(DATAFILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) 
    {
        perror("open");
        exit(1);
    }

    if(flock(fd, LOCK_EX) == -1) 
    {
        perror("flock");
        close(fd);
        exit(1);
    }

    //Read the whole file
    off_t size = lseek(fd, 0, SEEK_END);
    if(size == -1) 
    { 
        perror("lseek"); 
        exit(1); 
    }
    lseek(fd, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if(!content) 
    { 
        perror("malloc"); 
        exit(1); 
    }

    ssize_t n = read(fd, content, size);
    if(n < 0) 
    { 
        perror("read"); 
        exit(1); 
    }
    content[n] = '\0';

    //Build the new content
    char buffer[1024];
    int replaced = 0;
    char *out = NULL;
    size_t out_len = 0;

    char *line = strtok(content, "\n");
    while(line) 
    {
        char *colon = strchr(line, ':');
        if(colon) 
        {
            *colon = '\0';
            if(strcmp(line, key) == 0) 
            {
                //Replace value if it already exists
                int len = asprintf(&out, "%s%s:%s\n", out ? out : "", key, value);
                out_len = len;
                replaced = 1;
            } 
            else 
            {
                int len = asprintf(&out, "%s%s:%s\n", out ? out : "", line, colon + 1);
                out_len = len;
            }
        }
        line = strtok(NULL, "\n");
    }

    // If key not found, append new entry
    if(!replaced) 
    {
        int len = asprintf(&out, "%s%s:%s\n", out ? out : "", key, value);
        out_len = len;
    }

    // Truncate and rewrite
    if(ftruncate(fd, 0) == -1) 
    {
        perror("ftruncate");
        exit(1);
    }
    lseek(fd, 0, SEEK_SET);

    if(write(fd, out, out_len) != out_len) 
    {
        perror("write");
    }

    free(content);
    free(out);

    flock(fd, LOCK_UN);
    close(fd);
}

//Get value from key in database
void get(const char *key) 
{
    char buf[1024];
    int fd = open(DATAFILE, O_RDONLY);

    //Error opening file
    if(fd < 0) 
    { 
        perror("open"); 
        exit(1); 
    }

    //Acquire lock
    if(flock(fd, LOCK_EX) == -1) 
    { 
        perror("flock"); exit(1); 
    }

    ssize_t n;
    char line[1024];
    size_t pos = 0;

    //File parsing logic: check for new lines, colon separators, and null terminators to search for key
    while((n = read(fd, buf, sizeof(buf))) > 0) 
    {
        for(int i = 0; i < n; i++) 
        {
            if(buf[i] == '\n') 
            {
                line[pos] = '\0';
                char *colon = strchr(line, ':');

                if(colon) 
                {
                    *colon = '\0'; //Sets colon to null terminator
                    if(strcmp(line, key) == 0) 
                    {
                        printf("%s\n", colon + 1); //Return our value and exit
                        close(fd);
                        return;
                    }
                }
                pos = 0; //Reset line position
            }

            //Iterating through line
            else if(pos < sizeof(line) - 1) 
            {
                line[pos++] = buf[i];
            }
        }
    }

    //If not found
    fprintf(stderr, "Key not found\n");
    flock(fd, LOCK_UN); //Release lock
    close(fd);
}

//Rewrite database file without duplicate or deleted entries to save space
void compact(void) 
{
    FILE *src = fopen(DATAFILE, "r");
    FILE *tmp = fopen("data.tmp", "w");
    if(!src || !tmp) 
    {
        perror("compact fopen");
        return;
    }

    char line[1024];
    char seen_keys[1024][128];
    int seen_count = 0;

    while(fgets(line, sizeof(line), src)) 
    {
        char key[128], value[896];
        if(sscanf(line, "%127[^:]:%895[^\n]", key, value) == 2) 
        {
            int found = 0;
            for(int i = 0; i < seen_count; i++) 
            {
                if(strcmp(seen_keys[i], key) == 0) 
                {
                    found = 1;
                    break;
                }
            }
            if(!found) 
            {
                strcpy(seen_keys[seen_count++], key);
                fprintf(tmp, "%s:%s\n", key, value);
            }
        }
    }

    fclose(src);
    fclose(tmp);
    rename("data.tmp", DATAFILE);
}

