#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

volatile sig_atomic_t keep_running = 1;

void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) keep_running = 0;
    if (signo == SIGHUP) syslog(LOG_INFO, "Configuration reload requested.");
    if (signo == SIGCHLD) while (waitpid(-1, NULL, WNOHANG) > 0);
}

void run_as_daemon() {
    pid_t p = fork();
    if (p < 0) exit(EXIT_FAILURE);
    if (p > 0) exit(EXIT_SUCCESS);
    setsid();
    
    p = fork();
    if (p < 0) exit(EXIT_FAILURE);
    if (p > 0) exit(EXIT_SUCCESS);
    
    chdir("/");
    umask(022);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int is_user_allowed(const char *uname) {
    FILE *f = fopen("/etc/myRPC/users.conf", "r");
    if (!f) return 0;
    char buffer[256];
    int allowed = 0;
    while (fscanf(f, "%255s", buffer) == 1) {
        if (strcmp(buffer, uname) == 0) {
            allowed = 1;
            break;
        }
    }
    fclose(f);
    return allowed;
}

void process_client_request(const char *u_name, const char *cmd_text, int sock_fd) {
    if (!is_user_allowed(u_name)) {
        write(sock_fd, "1: \"Access Denied\"", 18);
        return;
    }

    char out_tpl[] = "/tmp/myRPC_XXXXXX.stdout";
    char err_tpl[] = "/tmp/myRPC_XXXXXX.stderr";
    int f_out = mkstemp(out_tpl);
    int f_err = mkstemp(err_tpl);

    pid_t child = fork();
    if (child == 0) {
        dup2(f_out, STDOUT_FILENO);
        dup2(f_err, STDERR_FILENO);
        execl("/bin/bash", "bash", "-c", cmd_text, (char *)NULL);
        exit(1);
    }

    int stat_val;
    waitpid(child, &stat_val, 0);

    char reply_msg[8192];
    char file_buf[4096];
    memset(file_buf, 0, sizeof(file_buf));

    if (WIFEXITED(stat_val) && WEXITSTATUS(stat_val) == 0) {
        lseek(f_out, 0, SEEK_SET);
        read(f_out, file_buf, 4095);
        sprintf(reply_msg, "0: \"%s\"", file_buf);
    } else {
        lseek(f_err, 0, SEEK_SET);
        read(f_err, file_buf, 4095);
        sprintf(reply_msg, "1: \"%s\"", file_buf);
    }

    write(sock_fd, reply_msg, strlen(reply_msg));
    close(f_out);
    close(f_err);
    unlink(out_tpl);
    unlink(err_tpl);
}

int main(int argc, char **argv) {
    openlog("RPC_DAEMON", LOG_PID, LOG_USER);

    int listen_port = 1234;
    FILE *cfg = fopen("/etc/myRPC/myRPC.conf", "r");
    if (cfg) {
        char k[50], eq[5], v[50];
        while (fscanf(cfg, "%49s %4s %49s", k, eq, v) == 3) {
            if (strcmp(k, "port") == 0) listen_port = atoi(v);
        }
        fclose(cfg);
    }

    if (argc == 2 && strcmp(argv[1], "--daemon") == 0) {
        run_as_daemon();
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGCHLD, &act, NULL);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(listen_port);

    bind(server_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    listen(server_sock, 10);
    syslog(LOG_INFO, "Listening on %d", listen_port);

    while (keep_running) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) continue;

        if (fork() == 0) {
            close(server_sock);
            char recv_buf[4096];
            memset(recv_buf, 0, sizeof(recv_buf));
            read(client_sock, recv_buf, sizeof(recv_buf) - 1);

            char parsed_user[256] = {0};
            char parsed_cmd[1024] = {0};
            sscanf(recv_buf, "\"%[^\"]\": \"%[^\"]\"", parsed_user, parsed_cmd); 		    process_client_request(parsed_user, parsed_cmd, client_sock);
            close(client_sock);
            exit(0);
        }
        close(client_sock);
    }
    
    close(server_sock);
    closelog();
    return 0;
}