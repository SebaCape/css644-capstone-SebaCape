#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define SOCKET_PATH "/tmp/db_socket"
#define BUF_SIZE 1024

//Explicit function declaration
void set(const char *key, const char *value);
void get(const char *key);
void size_command(void);

//Handle a single client connection
void handle_client(int client_fd) 
{
    char buf[BUF_SIZE];

    while (1) 
    {
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if(n <= 0) 
        {
            //Client disconnected
            break;
        }
        buf[n] = '\0';

        //Sleep to test multithreading
        sleep(5);

        // Parse command
        char *cmd = strtok(buf, " \n");
        if(!cmd)
            continue;

        if(strcmp(cmd, "quit") == 0) 
        {
            printf("[Server] Client requested quit\n");
            break;
        }
        else if(strcmp(cmd, "get") == 0) 
        {
            char *key = strtok(NULL, " \n");
            if(key) 
            {
                printf("[Server] Response to get %s: ", key);
                get(key);  //Output printed to server terminal
            } 
            else 
            {
                printf("[Server] Usage: get <key>\n");
            }
        }
        else if(strcmp(cmd, "set") == 0) 
        {
            char *key = strtok(NULL, " \n");
            char *value = strtok(NULL, "\n");
            if(key && value) 
            {
                set(key, value);
                printf("[Server] OK: set %s %s\n", key, value);
            } 
            else 
            {
                printf("[Server] Usage: set <key> <value>\n");
            }
        }
        else if(strcmp(cmd, "size") == 0) 
        {
            printf("[Server] Database size: ");
            size_command();  //Output printed to server terminal
        }
        else 
        {
            printf("[Server] Unknown command: %s\n", cmd);
        }
    }
}

void* client_thread(void* arg) 
{
    int client_fd = *(int*)arg;
    free(arg);

    handle_client(client_fd);
    close(client_fd);

    return NULL;
}

int main(void) 
{
    int server_fd, *client_fd;
    struct sockaddr_un addr;

    unlink(SOCKET_PATH);
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Server listening on %s\n", SOCKET_PATH);
    setvbuf(stdout, NULL, _IONBF, 0); //Disable buffering globally to prevent printing conflicts


    while(1) 
    {
        client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd == -1) 
        {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_fd) != 0) 
        {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    return 0;
}