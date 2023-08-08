#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#include <netinet/tcp.h>

void highLight(char *database_word, char *buffer);

#define COLOR_GREEN "\033[38;2;139;232;229m"
#define COLOR_YELLOW "\033[48;5;248;222;34m"
#define COLOR_RESET "\033[0m"
#define BUF_SIZE 1024

int main()
{
    highLight("Direct Hotel shop", "hotel");
}

void highLight(char *database_word, char *buffer)
{

    char str1[BUF_SIZE];
    char str2[BUF_SIZE];
    char temp[BUF_SIZE];
    char *ptr;
    char *ptr2;

    strcpy(str1, database_word);
    strcpy(str2, buffer);

    for (int i = 0; i < strlen(str2); i++)
        str2[i] = tolower(str2[i]);

    while (1)
    {
        ptr = strtok(str1, " ");
        if (str1 == NULL)
            break;
        ptr2 = ptr;
        for (int i = 0; i < strlen(ptr); i++)
            *ptr2 = tolower(*ptr2);
        
        if (strcmp(ptr2, str2) == 0)
            
    }

    char *ptr1 = str1;
    char *ptr2;

    ptr2 = strstr(str1, str2);

    while (1)
    {
        if (ptr1 == ptr2)
            break;
        ptr1++;
        point_index++;
    }

    for (int i = 0; i < point_index; i++)
        printf("%c", database_word[i]);

    printf("%s", COLOR_YELLOW);
    for (int i = 0; i < search_len; i++)
        printf("%c", database_word[point_index++]);
    printf("%s", COLOR_RESET);

    while (database_word[point_index] != '\0')
        printf("%c", database_word[point_index++]);

    printf("\n");
}
