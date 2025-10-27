#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#define SOCKET_PATH "/tmp/db_socket"
#define BUF_SIZE 1024

int main(void) 
{
    int sock;
    struct sockaddr_un addr;
    char buf[BUF_SIZE];

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
    {
        perror("connect");
        exit(1);
    }

    printf("Connected to DB. Type commands like:\n");
    printf("  set foo bar\n  get foo\n  size\n  quit\n");

    while(1) 
    {
        printf("> ");
        fflush(stdout);

        if(!fgets(buf, sizeof(buf), stdin)) 
            break;

        if(strncmp(buf, "quit", 4) == 0) 
            break;

        //Handle compact by sending signal to server
        if (strncmp(buf, "compact", 7) == 0) 
        {
        FILE *pidf = fopen("server.pid", "r");
            if (!pidf) 
            {
                perror("open pid");
                continue;
            }

            int pid;
            fscanf(pidf, "%d", &pid);
            fclose(pidf);
            kill(pid, SIGUSR1);
            printf("Sent SIGUSR1 to server (PID %d)\n", pid);
            continue;  //Skip socket writing
        }

        //Send commands to server
        write(sock, buf, strlen(buf));
    }

    close(sock);
    return 0;
}
