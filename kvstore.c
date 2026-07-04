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

    //Read database using stream parsing
    int src_fd = dup(fd);
    if(src_fd < 0)
    {
        perror("dup");
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    FILE *src = fdopen(src_fd, "r");
    if(!src)
    {
        perror("fdopen");
        close(src_fd);
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    FILE *tmp = tmpfile();
    if(!tmp)
    {
        perror("tmpfile");
        fclose(src);
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t line_len;
    int replaced = 0;
    size_t key_len = strlen(key);

    while((line_len = getline(&line, &cap, src)) != -1)
    {
        char *colon = strchr(line, ':');
        if(!colon)
        {
            if(fwrite(line, 1, (size_t)line_len, tmp) != (size_t)line_len)
            {
                perror("fwrite");
                free(line);
                fclose(tmp);
                fclose(src);
                flock(fd, LOCK_UN);
                close(fd);
                exit(1);
            }
            continue;
        }

        size_t current_key_len = (size_t)(colon - line);
        if(current_key_len == key_len && strncmp(line, key, key_len) == 0)
        {
            if(!replaced)
            {
                fprintf(tmp, "%s:%s\n", key, value);
                replaced = 1;
            }
            continue;
        }

        if(fwrite(line, 1, (size_t)line_len, tmp) != (size_t)line_len)
        {
            perror("fwrite");
            free(line);
            fclose(tmp);
            fclose(src);
            flock(fd, LOCK_UN);
            close(fd);
            exit(1);
        }
    }

    free(line);

    //Append key if not already present
    if(!replaced)
    {
        fprintf(tmp, "%s:%s\n", key, value);
    }

    //Truncate and rewrite from temporary stream
    if(ftruncate(fd, 0) == -1)
    {
        perror("ftruncate");
        fclose(tmp);
        fclose(src);
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    if(lseek(fd, 0, SEEK_SET) == -1)
    {
        perror("lseek");
        fclose(tmp);
        fclose(src);
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    rewind(tmp);
    char buf[4096];
    size_t nread;

    while((nread = fread(buf, 1, sizeof(buf), tmp)) > 0)
    {
        size_t written = 0;
        while(written < nread)
        {
            ssize_t nw = write(fd, buf + written, nread - written);
            if(nw < 0)
            {
                perror("write");
                fclose(tmp);
                fclose(src);
                flock(fd, LOCK_UN);
                close(fd);
                exit(1);
            }
            written += (size_t)nw;
        }
    }

    fclose(tmp);
    fclose(src);

    flock(fd, LOCK_UN);
    close(fd);
}

//Get value from key in database
void get(const char *key) 
{
    int fd = open(DATAFILE, O_RDONLY);

    //Error opening file
    if(fd < 0) 
    { 
        perror("open"); 
        exit(1); 
    }

    //Acquire shared lock for read operations
    if(flock(fd, LOCK_SH) == -1) 
    { 
        perror("flock");
        close(fd);
        exit(1); 
    }

    //Read database line by line and match key
    int src_fd = dup(fd);
    if(src_fd < 0)
    {
        perror("dup");
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    FILE *src = fdopen(src_fd, "r");
    if(!src)
    {
        perror("fdopen");
        close(src_fd);
        flock(fd, LOCK_UN);
        close(fd);
        exit(1);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t line_len;
    size_t key_len = strlen(key);

    while((line_len = getline(&line, &cap, src)) != -1)
    {
        char *colon = strchr(line, ':');
        if(!colon)
            continue;

        size_t current_key_len = (size_t)(colon - line);
        if(current_key_len != key_len || strncmp(line, key, key_len) != 0)
            continue;

        char *value = colon + 1;
        if(line_len > 0 && value[line_len - (current_key_len + 1) - 1] == '\n')
            value[line_len - (current_key_len + 1) - 1] = '\0';

        printf("%s\n", value);
        free(line);
        fclose(src);
        flock(fd, LOCK_UN);
        close(fd);
        return;
    }

    free(line);
    fclose(src);

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

