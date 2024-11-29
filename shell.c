#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 1024
#define HISTORY_FILE "history_log.txt"
#define VFS_FILE "/tmp/cron_vfs"

char *command_history[100];
int history_count = 0;

// Функция для загрузки истории команд
void load_command_history() {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (file == NULL) return;
    char line[MAX_COMMAND_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; // Убираем символ новой строки
        command_history[history_count++] = strdup(line);
    }
    fclose(file);
}

// Функция для сохранения истории команд
void save_command_history() {
    FILE *file = fopen(HISTORY_FILE, "w");
    if (file == NULL) {
        perror("Ошибка сохранения истории");
        return;
    }
    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s\n", command_history[i]);
    }
    fclose(file);
}

// Обработка сигнала SIGHUP
void sighup_handler(int signum) {
    if (signum == SIGHUP) {
        printf("Configuration reloaded.\n");
    }
}


// Функция для проверки загрузочного сектора
void check_bootable(const char *device) {
    char path[256];
    snprintf(path, sizeof(path), "/dev/%s", device);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Ошибка открытия устройства");
        return;
    }

    unsigned char sector[512];
    if (read(fd, sector, 512) != 512) {
        perror("Ошибка чтения первого сектора");
        close(fd);
        return;
    }
    close(fd);

    if (sector[510] == 0x55 && sector[511] == 0xAA) {
        printf("Устройство %s является загрузочным (сигнатура 0xAA55).\n", device);
    } else {
        printf("Устройство %s не является загрузочным.\n", device);
    }
}

// Создание VFS для задач cron
void manage_cron_tasks() {
    FILE *vfs_file = fopen(VFS_FILE, "w");
    if (vfs_file == NULL) {
        perror("Ошибка создания VFS");
        return;
    }

    system("ls /var/spool/cron > /tmp/cron_vfs 2>/dev/null");
    printf("VFS для задач cron создан в %s.\n", VFS_FILE);
    fclose(vfs_file);
}

// Создание дампа памяти процесса
void create_memory_dump(int pid) {
    char command[MAX_COMMAND_LENGTH];
    snprintf(command, sizeof(command), "gdb --batch -p %d -ex 'gcore /tmp/mem_dump_%d' -ex 'detach' -ex 'quit'", pid, pid);
    int status = system(command);
    if (status == 0) {
        printf("Дамп памяти процесса %d создан: /tmp/mem_dump_%d\n", pid, pid);
    } else {
        printf("Ошибка создания дампа памяти процесса %d.\n", pid);
    }
}

// Функция для обработки команды
void process_command(char *command) {
    if (strcmp(command, "exit") == 0 || strcmp(command, "\\q") == 0) {
        save_command_history();
        printf("До свидания!\n");
        exit(0);
    }

    if (strncmp(command, "echo ", 5) == 0) {
        printf("%s\n", command + 5);
        return;
    }

    if (strcmp(command, "history") == 0) {
        for (int i = 0; i < history_count; i++) {
            printf("%d: %s\n", i + 1, command_history[i]);
        }
        return;
    }

    if (strncmp(command, "\\e ", 3) == 0) {
        char *env_var = command + 3;
        char *value = getenv(env_var + 1); // Пропускаем символ $
        if (value) {
            printf("%s=%s\n", env_var, value);
        } else {
            printf("Переменная окружения %s не найдена.\n", env_var);
        }
        return;
    }

    if (strncmp(command, "\\l ", 3) == 0) {
        char *device = command + 3;
        check_bootable(device);
        return;
    }

    if (strcmp(command, "\\cron") == 0) {
        manage_cron_tasks();
        return;
    }

    if (strncmp(command, "\\mem ", 5) == 0) {
        int pid = atoi(command + 5);
        create_memory_dump(pid);
        return;
    }

    // Выполнение внешней команды
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"/bin/sh", "-c", command, NULL};
        execvp(args[0], args);
        perror("Ошибка выполнения команды");
        exit(1);
    } else {
        wait(NULL);
    }
}

int main() {
    signal(SIGHUP, sighup_handler);
    load_command_history();

    printf("Привет в моем шелле. Используйте 'exit' или '\\q' для выхода.\n");

    char command[MAX_COMMAND_LENGTH];
    while (1) {
        printf("> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("\n");
            break;
        }
        command[strcspn(command, "\n")] = 0; // Убираем символ новой строки

        if (strlen(command) == 0) continue;

        // Сохраняем команду в историю
        if (history_count < 100) {
            command_history[history_count++] = strdup(command);
        }

        process_command(command);
    }

    save_command_history();
    return 0;
}

