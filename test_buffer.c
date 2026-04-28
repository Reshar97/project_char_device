#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include "my_buffer_ioctl.h"

void print_help() {
    printf("\nДоступные команды:\n");
    printf("  write <текст> - Записать строку в буфер\n");
    printf("  read          - Прочитать всё содержимое буфера\n");
    printf("  size          - Общий размер буфера\n");
    printf("  used          - Сколько байт занято\n");
    printf("  free          - Сколько байт свободно\n");
    printf("  clear         - Очистить буфер\n");
    printf("  exit          - Выход\n");
}

int main() {
    int fd = open("/dev/my_buffer", O_RDWR);
    if (fd < 0) {
        perror("Ошибка открытия /dev/my_buffer");
        return 1;
    }

    char cmd[32];
    char buffer[1024];
    int32_t val;

    printf("=== Интерактивная консоль RingBuffer ===\n");
    print_help();

    while (1) {
        printf("\nring_buffer> ");
        if (scanf("%31s", cmd) != 1) break;

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "write") == 0) {
            // Читаем остаток строки после команды "write"
            fgets(buffer, sizeof(buffer), stdin);
            // Убираем лишний пробел в начале и символ переноса в конце
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
            
            ssize_t written = write(fd, buffer + 1, strlen(buffer + 1));
            printf("Успешно записано %zd байт.\n", written);
        } 
        else if (strcmp(cmd, "read") == 0) {
            ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                printf("Данные из буфера: [%s]\n", buffer);
            } else {
                printf("Буфер пуст.\n");
            }
        }
        else if (strcmp(cmd, "size") == 0) {
            ioctl(fd, IOCTL_GETBUFFER_SIZE, &val);
            printf("Размер: %d байт\n", val);
        }
        else if (strcmp(cmd, "used") == 0) {
            ioctl(fd, IOCTL_GET_USED_SPACE, &val);
            printf("Занято: %d байт\n", val);
        }
        else if (strcmp(cmd, "free") == 0) {
            ioctl(fd, IOCTL_GET_FREE_SPACE, &val);
            printf("Свободно: %d байт\n", val);
        }
        else if (strcmp(cmd, "clear") == 0) {
            ioctl(fd, IOCTL_CLEAR_BUFFER);
            printf("Буфер очищен.\n");
        }
        else {
            printf("Неизвестная команда. Введи 'help'.\n");
            print_help();
        }
    }

    close(fd);
    return 0;
}

