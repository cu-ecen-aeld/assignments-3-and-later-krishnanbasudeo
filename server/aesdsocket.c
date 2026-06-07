#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define RECV_CHUNK_SIZE 1024

static volatile sig_atomic_t exit_requested = 0;
static int server_fd = -1;
static int client_fd = -1;

static void signal_handler(int signal_number)
{
    (void)signal_number;
    exit_requested = 1;

    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
    }

    if (client_fd != -1) {
        shutdown(client_fd, SHUT_RDWR);
    }
}

static int setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        return -1;
    }

    return 0;
}

static int setup_server_socket(void)
{
    struct addrinfo hints;
    struct addrinfo *server_info = NULL;
    struct addrinfo *p = NULL;
    int socket_fd = -1;
    int yes = 1;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, PORT, &hints, &server_info);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));
        return -1;
    }

    for (p = server_info; p != NULL; p = p->ai_next) {
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
            close(socket_fd);
            socket_fd = -1;
            continue;
        }

        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(server_info);

    if (socket_fd == -1) {
        return -1;
    }

    if (listen(socket_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

static bool append_packet_to_file(const char *packet, size_t packet_len)
{
    FILE *fp = fopen(DATA_FILE, "a");
    if (fp == NULL) {
        syslog(LOG_ERR, "fopen append failed: %s", strerror(errno));
        return false;
    }

    if (fwrite(packet, 1, packet_len, fp) != packet_len) {
        syslog(LOG_ERR, "fwrite failed: %s", strerror(errno));
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

static bool send_file_to_client(int socket_fd)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char buffer[RECV_CHUNK_SIZE];

    if (fp == NULL) {
        syslog(LOG_ERR, "fopen read failed: %s", strerror(errno));
        return false;
    }

    while (!feof(fp)) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);

        if (bytes_read > 0) {
            size_t total_sent = 0;

            while (total_sent < bytes_read) {
                ssize_t bytes_sent = send(socket_fd,
                                          buffer + total_sent,
                                          bytes_read - total_sent,
                                          0);

                if (bytes_sent == -1) {
                    syslog(LOG_ERR, "send failed: %s", strerror(errno));
                    fclose(fp);
                    return false;
                }

                total_sent += (size_t)bytes_sent;
            }
        }

        if (ferror(fp)) {
            syslog(LOG_ERR, "fread failed: %s", strerror(errno));
            fclose(fp);
            return false;
        }
    }

    fclose(fp);
    return true;
}

static bool handle_client(int socket_fd)
{
    char recv_buffer[RECV_CHUNK_SIZE];
    char *packet = NULL;
    size_t packet_size = 0;
    bool success = true;

    while (!exit_requested) {
        ssize_t bytes_received = recv(socket_fd, recv_buffer, sizeof(recv_buffer), 0);

        if (bytes_received == 0) {
            break;
        }

        if (bytes_received == -1) {
            if (errno == EINTR && exit_requested) {
                break;
            }

            syslog(LOG_ERR, "recv failed: %s", strerror(errno));
            success = false;
            break;
        }

        char *new_packet = realloc(packet, packet_size + (size_t)bytes_received);
        if (new_packet == NULL) {
            syslog(LOG_ERR, "realloc failed");
            free(packet);
            return false;
        }

        packet = new_packet;
        memcpy(packet + packet_size, recv_buffer, (size_t)bytes_received);
        packet_size += (size_t)bytes_received;

        char *newline = memchr(packet, '\n', packet_size);
        if (newline != NULL) {
            size_t complete_packet_len = (size_t)(newline - packet) + 1;

            if (!append_packet_to_file(packet, complete_packet_len)) {
                success = false;
                break;
            }

            if (!send_file_to_client(socket_fd)) {
                success = false;
                break;
            }

            size_t remaining_len = packet_size - complete_packet_len;
            if (remaining_len > 0) {
                memmove(packet, packet + complete_packet_len, remaining_len);
            }

            packet_size = remaining_len;

            char *resized_packet = NULL;
            if (packet_size > 0) {
                resized_packet = realloc(packet, packet_size);
                if (resized_packet != NULL) {
                    packet = resized_packet;
                }
            } else {
                free(packet);
                packet = NULL;
            }

            break;
        }
    }

    free(packet);
    return success;
}

static int daemonize_process(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        return -1;
    }

    if (chdir("/") == -1) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        return -1;
    }

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);

        if (dev_null > STDERR_FILENO) {
            close(dev_null);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    } else if (argc > 1) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return -1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (setup_signal_handlers() != 0) {
        syslog(LOG_ERR, "signal setup failed: %s", strerror(errno));
        closelog();
        return -1;
    }

    server_fd = setup_server_socket();
    if (server_fd == -1) {
        closelog();
        return -1;
    }

    if (daemon_mode) {
        if (daemonize_process() != 0) {
            close(server_fd);
            closelog();
            return -1;
        }
    }

    while (!exit_requested) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char client_ip[INET_ADDRSTRLEN] = {0};

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_fd == -1) {
            if (exit_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        struct sockaddr_in *client_addr_in = (struct sockaddr_in *)&client_addr;
        inet_ntop(AF_INET, &(client_addr_in->sin_addr), client_ip, sizeof(client_ip));

        syslog(LOG_DEBUG, "Accepted connection from %s", client_ip);

        handle_client(client_fd);

        close(client_fd);
        client_fd = -1;

        syslog(LOG_DEBUG, "Closed connection from %s", client_ip);
    }

    syslog(LOG_DEBUG, "Caught signal, exiting");

    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }

    remove(DATA_FILE);
    closelog();

    return 0;
}
