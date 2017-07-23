#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **parse_args(char *);

int 
listen_socket()
{

    pid_t pid;
    char char_pid[20];
    int fd_socket;
    struct sockaddr_un addr;

    pid = getpid();

    if ( (fd_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket error");
            exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    sprintf(char_pid, "sockets/%d", pid);
    strncpy(addr.sun_path, char_pid, strlen(char_pid));

    if (bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind error");
        exit(-1);
    }

    if (listen(fd_socket, 0) == -1) {
        perror("listen error");
        exit(-1);
    }

    return (fd_socket);

}

int
accept_socket(int fd_socket)
{

    int socket_in;

    if ( (socket_in = accept(fd_socket, NULL, NULL)) == -1 ) {
        perror("accept");
    }

    return (socket_in);

}

int
send_bitmap(int fd_socket, char *bitmap, int len_bitmap)
{

    int wb;

    wb = write(fd_socket, bitmap, len_bitmap);

    return (wb);

}

char *
receive_parameter(int fd_socket)
{
    
    int rc;
    char *argv = malloc(sizeof(char) * 100);
    char buffer[100];

    rc = read(fd_socket, buffer, sizeof(buffer));

    if ( rc <= 0 )
        return NULL;

    strncpy(argv, buffer, rc);
    argv[rc] = 0x00;

    return (argv);

}
