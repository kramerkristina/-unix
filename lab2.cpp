#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

volatile sig_atomic_t was_sighup = 0;
volatile sig_atomic_t was_sigint = 0;

static void signal_handler(int signal_number) {
    switch (signal_number) {
        case SIGHUP:
            was_sighup = 1;
            break;
        case SIGINT:
            was_sigint = 1;
            break;
        default:
            break;
    }
}

int main(void) {
    int port = 9090;
    setvbuf(stdout, NULL, _IOLBF, 0);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGHUP, &action, NULL) == -1 || sigaction(SIGINT, &action, NULL) == -1) {
        fprintf(stderr, "Ошибка sigaction: %s\n", strerror(errno));
        return 1;
    }

    sigset_t blocked_mask;
    sigset_t original_mask;
    sigemptyset(&blocked_mask);
    sigaddset(&blocked_mask, SIGHUP);
    sigaddset(&blocked_mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &blocked_mask, &original_mask) == -1) {
        fprintf(stderr, "Ошибка sigprocmask: %s\n", strerror(errno));
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    if (server_fd < 0 || setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        fprintf(stderr, "Ошибка создания сокета: %s\n", strerror(errno));
        if (server_fd >= 0) close(server_fd);
        return 1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1 ||
        listen(server_fd, 16) == -1) {
        fprintf(stderr, "Ошибка запуска сокета: %s\n", strerror(errno));
        close(server_fd);
        return 1;
    }

    printf("Сервер запущен на порту %d\n", port);

    int client_fd = -1;
    char buffer[4096];

    while (!was_sigint) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        if (client_fd != -1) FD_SET(client_fd, &read_fds);
        int max_fd = server_fd > client_fd ? server_fd : client_fd;

        int ready = pselect(max_fd + 1, &read_fds, NULL, NULL, NULL, &original_mask);

        if (was_sighup) {
            printf("Получен сигнал SIGHUP\n");
            was_sighup = 0;
        }

        if (ready > 0 && client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
            if (received > 0) {
                printf("Получено байт: %zd\n", received);
            } else if (received == 0 || errno != EINTR) {
                if (received == 0) printf("Клиент отключился\n");
                else fprintf(stderr, "Ошибка чтения: %s\n", strerror(errno));
                close(client_fd);
                client_fd = -1;
            }
        }

        if (ready > 0 && FD_ISSET(server_fd, &read_fds)) {
            int new_fd = accept(server_fd, NULL, NULL);
            if (new_fd < 0) {
                if (errno != EINTR) fprintf(stderr, "Ошибка accept: %s\n", strerror(errno));
            } else if (client_fd != -1) {
                printf("Лишнее подключение отклонено\n");
                close(new_fd);
            } else {
                printf("Новое подключение принято\n");
                client_fd = new_fd;
            }
        }
    }

    printf("Получен сигнал SIGINT, завершение работы\n");
    if (client_fd != -1) close(client_fd);
    close(server_fd);
    sigprocmask(SIG_SETMASK, &original_mask, NULL);
    return 0;
}
