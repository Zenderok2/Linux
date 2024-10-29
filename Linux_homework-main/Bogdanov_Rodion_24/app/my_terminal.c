#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

void handle_sighup(int sig)
{
    printf("Configuration reloaded\n");
}

void dump_memory(int pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", pid);

    int mem_fd = open(path, O_RDONLY);
    if (mem_fd == -1)
    {
        perror("Ошибка открытия файла памяти процесса");
        return;
    }

    char dump_path[128];
    snprintf(dump_path, sizeof(dump_path), "/tmp/memory_dump_%d.txt", pid);

    int dump_fd = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dump_fd == -1)
    {
        perror("Ошибка создания дамп-файла");
        close(mem_fd);
        return;
    }

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(mem_fd, buffer, sizeof(buffer))) > 0)
    {
        if (write(dump_fd, buffer, bytes_read) != bytes_read)
        {
            perror("Ошибка записи в дамп-файл");
            break;
        }
    }

    if (bytes_read == -1)
        perror("Ошибка чтения памяти процесса");

    close(mem_fd);
    close(dump_fd);
}

void print_partition_info()
{
    FILE *file = fopen("/proc/partitions", "r");
    if (!file)
    {
        perror("Ошибка открытия /proc/partitions");
        return;
    }

    char line[256];
    printf("Информация о разделах:\n");
    while (fgets(line, sizeof(line), file))
    {
        if (strstr(line, "sda") || strstr(line, "sdb"))
        {
            printf("%s", line);
        }
    }

    fclose(file);
}

void execute_binary(const char *path)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("Ошибка создания процесса");
        return;
    }

    if (pid == 0)
    {
        execl(path, path, (char *)NULL);
        perror("Ошибка выполнения бинарного файла");
        exit(EXIT_FAILURE);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            printf("Программа завершена с кодом %d\n", WEXITSTATUS(status));
        else
            printf("Программа завершилась ненормально\n");
    }
}

int main()
{
    char input[1024];
    FILE *history = fopen("history.txt", "a");

    signal(SIGHUP, handle_sighup);

    while (1)
    {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\nЗавершение программы.\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        fprintf(history, "%s\n", input);
        fflush(history);

        if (strcmp(input, "exit") == 0 || strcmp(input, "\\q") == 0)
        {
            printf("Выход\n");
            break;
        }

        else if (strncmp(input, "echo ", 5) == 0)
        {
            printf("%s\n", input + 5);
        }

        else if (strncmp(input, "\\e $", 4) == 0)
        {
            char *var = getenv(input + 4);
            if (var)
            {
                printf("%s\n", var);
            }
            else
            {
                printf("Переменная не найдена\n");
            }
        }

        else if (strncmp(input, "./", 2) == 0)
        {
            execute_binary(input);
        }

        else if (strcmp(input, "\\l /dev/sda") == 0)
        {
            print_partition_info();
        }

        else if (strncmp(input, "\\mem ", 5) == 0)
        {
            int pid = atoi(input + 5);
            if (pid > 0)
            {
                dump_memory(pid);
                printf("Дамп памяти процесса %d сохранен в /tmp.\n", pid);
            }
            else
            {
                printf("Некорректный PID.\n");
            }
        }

        else
        {
            printf("Неизвестная команда: %s\n", input);
        }
    }

    fclose(history);
    return 0;
}

