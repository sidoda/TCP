#ifndef DATATYPE
#define DATATYPE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define CHAR_SIZE 26
#define NAME_SIZE 256
#define WORD_SIZE 256

typedef struct Trie
{
    int isLeaf;
    int search_num;
    char data[NAME_SIZE];
    struct Trie *character[CHAR_SIZE];
} Trie;

typedef struct search_node
{
    int search_num;
    char search_word[NAME_SIZE];
    struct search_node *next;
} search_node;

typedef struct
{
    char search_word[WORD_SIZE];
    int search_num;
} search_info_t;

Trie *getNewTrieNode();
void TireInsert(Trie *head, char *search_word, int search_num);
void search(Trie *head, char *search_word, search_info_t **info_pkt, int *data_size);
void searchIn(Trie *cur, char *lower_search_word_rev, char *ptr, int cond, search_info_t **info_pkt, int *data_size);
void revstr(char *str1);
void showTrie(Trie *head);
void selectionSort(search_info_t **info_pkt, int *data_size);

Trie *getNewTrieNode()
{
    Trie *node = malloc(sizeof(Trie));

    node->isLeaf = 0;
    for (int i = 0; i < CHAR_SIZE; i++)
        node->character[i] = NULL;
    node->search_num = 0;

    return node;
}

void TireInsert(Trie *head, char *search_word, int search_num)
{
    Trie *curr = head;
    char *str = search_word + strlen(search_word) - 1; // suffix tree
    char lower_alph;

    for (int i = 0; i < strlen(search_word); i++)
    {
        if (*str == ' ') // pass the withe space
        {
            while (*str == ' ')
            {
                str--;
                i++;
            }
        }

        lower_alph = tolower(*str); // save for lowercase only

        if (curr->character[lower_alph - 'a'] == NULL)
            curr->character[lower_alph - 'a'] = getNewTrieNode();

        curr = curr->character[lower_alph - 'a'];

        str--;
    }

    curr->isLeaf = 1;
    strcpy(curr->data, search_word);
    curr->search_num = search_num;
}

void search(Trie *head, char *search_word, search_info_t **info_pkt, int *data_size)
{
    Trie *cur = head;
    char search_temp[NAME_SIZE];
    char lower_search_word_rev[NAME_SIZE];
    char *ptr;

    // remove ' '
    strcpy(search_temp, search_word);
    ptr = search_word;
    int index = 0;
    while (1)
    {
        if (*ptr == '\0')
        {
            search_temp[index] = '\0';
            break;
        }

        else if (*ptr == ' ')
        {
            ptr++;
        }

        else
        {
            search_temp[index] = *ptr;
            index++;
            ptr++;
        }
    }

    // make search_word to lowercase and reverse
    strcpy(lower_search_word_rev, search_temp);
    for (int i = 0; i < strlen(search_temp); i++)
    {
        lower_search_word_rev[i] = tolower(lower_search_word_rev[i]);
    }

    revstr(lower_search_word_rev);

    ptr = lower_search_word_rev;

    if (cur == NULL) // if Trie is empty
        return;
    for (int i = 0; i < CHAR_SIZE; i++)
    {
        if (cur->character[i] != NULL)
        {

            if (i == *ptr - 'a')
            {
                searchIn(cur->character[i], lower_search_word_rev, ptr + 1, 0, info_pkt, data_size);
            }
            else
            {
                searchIn(cur->character[i], lower_search_word_rev, lower_search_word_rev, 0, info_pkt, data_size);
            }
        }
    }
}

void searchIn(Trie *cur, char *lower_search_word_rev, char *ptr, int cond, search_info_t **info_pkt, int *data_size)
{
    if (*ptr == '\0')
    {
        cond = 1;
        ptr = lower_search_word_rev;
    }
    if (cur->isLeaf == 1 && cond == 1)
    {
        strcpy(info_pkt[*data_size]->search_word, cur->data);
        info_pkt[*data_size]->search_num = cur->search_num;
        *data_size += 1;
    }

    for (int i = 0; i < CHAR_SIZE; i++)
    {
        if (cur->character[i] != NULL)
        {
            if (i == *ptr - 'a')
            {
                searchIn(cur->character[i], lower_search_word_rev, ptr + 1, cond, info_pkt, data_size);
            }
            else
            {
                searchIn(cur->character[i], lower_search_word_rev, lower_search_word_rev, cond, info_pkt, data_size);
            }
        }
    }
}

void revstr(char *str1)
{
    // declare variable
    int i, len, temp;
    len = strlen(str1); // use strlen() to get the length of str string

    // use for loop to iterate the string
    for (i = 0; i < len / 2; i++)
    {
        // temp variable use to temporary hold the string
        temp = str1[i];
        str1[i] = str1[len - i - 1];
        str1[len - i - 1] = temp;
    }
}

void showTrie(Trie *head)
{
    Trie *cur = head;

    if (cur->isLeaf == 1)
        printf("%s %d \n", cur->data, cur->search_num);

    for (int i = 0; i < CHAR_SIZE; i++)
    {
        if (cur->character[i] != NULL)
        {
            showTrie(cur->character[i]);
        }
    }
}

void selectionSort(search_info_t **info_pkt, int *data_size)
{
    int biggest;
    search_info_t *temp = malloc(sizeof(search_info_t));
    for (int i = 0; i < *data_size; i++)
    {
        biggest = i;
        for (int j = i + 1; j < *data_size; j++)
        {
            if (info_pkt[j]->search_num > info_pkt[biggest]->search_num)
                biggest = j;
        }

        if (i != biggest)
        {
            memcpy(temp, info_pkt[i], sizeof(search_info_t));
            memcpy(info_pkt[i], info_pkt[biggest], sizeof(search_info_t));
            memcpy(info_pkt[biggest], temp, sizeof(search_info_t));
        }
    }

    free(temp);
}
#endif
