#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DATAFILE "data.db"

//Set new key and value in our database
void set(const char *key, const char *value) 
{
    //Data Format - key:value\n
    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer), "%s:%s\n", key, value);
    int fd = open(DATAFILE, O_WRONLY | O_APPEND | O_CREAT, 0666);

    //Error opening file
    if(fd < 0) 
    { 
        perror("open"); 
        exit(1); 
    }

    //Error with write syscall
    if(write(fd, buffer, len) != len) 
    { 
        perror("write"); 
    }
    close(fd);
}

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

    ssize_t n;
    char line[1024];
    size_t pos = 0;

    //File parsing logic: check for new lines, colon separators, and null terminators to search for key
    while ((n = read(fd, buf, sizeof(buf))) > 0) 
    {
        for (int i = 0; i < n; i++) 
        {
            if (buf[i] == '\n') 
            {
                line[pos] = '\0';
                char *colon = strchr(line, ':');

                if (colon) 
                {
                    *colon = '\0'; //Sets colon to null terminator
                    if (strcmp(line, key) == 0) 
                    {
                        printf("%s\n", colon + 1); //Return our value and exit
                        close(fd);
                        return;
                    }
                }
                pos = 0; //Reset line position
            }

            //Iterating through line
            else if (pos < sizeof(line) - 1) 
            {
                line[pos++] = buf[i];
            }
        }
    }

    //If not found
    fprintf(stderr, "Key not found\n");
    close(fd);
}

//Main function
int main(int argc, char *argv[]) 
{
    //Usage Handling
    if(argc < 2) 
    {
        printf("Usage: set <key> <value> | get <key>\n");
        return 1;
    }

    //Call set functionality
    if(strcmp(argv[1], "set") == 0 && argc == 4) 
    {
        set(argv[2], argv[3]);
    }

    //Call get functionality
    else if(strcmp(argv[1], "get") == 0 && argc == 3) 
    {
        get(argv[2]);
    }

    //Invalid input
    else 
    {
        printf("Invalid input\n");
        return 1;
    }

    return 0;
}