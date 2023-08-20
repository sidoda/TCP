#ifndef _LINKED_LIST_H
#define _LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    int seq;
    char *content;
    int cur_size;
} data_pkt_t;

struct node
{
    data_pkt_t data;
    struct node *next;
};

void insert(struct node **head_ref, struct node *new_node);
void delete(struct node **head_ref);
struct node *newNode(int seq, char *content, int cur_size);
void printList(struct node *head);
int getTopSeq(struct node *head);
int getSize(struct node *head);

// linked-list function
void insert(struct node **head_ref, struct node *new_node)
{
    struct node *cur;

    if (*head_ref == NULL || (*head_ref)->data.seq >= new_node->data.seq)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }

    else
    {
        cur = *head_ref;
        while (cur->next != NULL && cur->next->data.seq < new_node->data.seq)
            cur = cur->next;
        new_node->next = cur->next;
        cur->next = new_node;
    }
}
void delete(struct node **head_ref)
{
    if (*head_ref == NULL)
        return;

    struct node *temp = *head_ref;

    *head_ref = (*head_ref)->next;

    free(temp->data.content);
    free(temp);
}
struct node *newNode(int seq, char *content, int cur_size)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));

    new_node->data.seq = seq;
    new_node->data.cur_size = cur_size;
    new_node->data.content = malloc(sizeof(char) * cur_size);
    memcpy(new_node->data.content, content, cur_size);

    new_node->next = NULL;

    return new_node;
}
void printList(struct node *head)
{
    struct node *temp = head;
    while (temp != NULL)
    {
        printf("seq: %d cur_size: %d \n", temp->data.seq, temp->data.cur_size);

        for (int i = 0; i < temp->data.cur_size; i++)
        {
            printf("%c", temp->data.content[i]);
        }
        printf("\n");
        temp = temp->next;
    }
}
int getTopSeq(struct node *head)
{
    if (head == NULL)
        return -1;
    struct node *temp = head;

    return temp->data.seq;
}
int getSize(struct node *head)
{
    struct node *cur = head;
    int count = 0;

    while (cur != NULL)
    {
        count++;
        cur = cur->next;
    }

    return count;
}

#endif
