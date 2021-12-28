#include <stdio.h>
#include <stdlib.h>

// struct Node
// {
//     int key;
//     struct Node *next;
// };

struct Node
{
    unsigned int index;
    unsigned int tag;
    unsigned int way;
    struct Node *next;
};

void push(struct Node **head_ref, unsigned int new_index, unsigned int new_tag, unsigned int new_way)
{
    struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));

    new_node->index = new_index;
    new_node->tag = new_tag;
    new_node->way = new_way;
    new_node->next = (*head_ref);
    (*head_ref) = new_node;
}

int search(struct Node *head, unsigned int index) //returns key where index is
{
    struct Node *current = head; // Initialize current
    while (current != NULL)
    {
        if (current->index == index)
            return 1;
        current = current->next;
    }
    return 0;
}

int checkHit(struct Node *head, unsigned int index, unsigned int tag)
{                                //returns 1 if hit, 0 if miss
    struct Node *current = head; // Initialize current
    while (current != NULL)
    {
        if (current->index == index && current->tag == tag)
        {
            //printf("Hit\n");
            return 1;
        }
        current = current->next;
    }
    //printf("Miss\n");
    return 0;
}

int getWay(struct Node *head, unsigned int index, unsigned int tag)
{
    struct Node *current = head; // Initialize current
    while (current != NULL)
    {
        if (current->index == index && current->tag == tag)
        {
            //printf("Hit\n");
            return current->way;
        }
        current = current->next;
    }
}

int countIndex(struct Node *head, unsigned int index) //returns how many same index there are
{
    int count = 0;
    struct Node *current = head; // Initialize current
    while (current != NULL)
    {
        if (current->index == index)
        {
            count++;
        }
        current = current->next;
    }
    return count;
}

void deleteNode(struct Node **head_ref, int position)
{
    // If linked list is empty
    if (*head_ref == NULL)
        return;

    // Store head node
    struct Node *temp = *head_ref;

    // If head needs to be removed
    if (position == 0)
    {
        *head_ref = temp->next; // Change head
        free(temp);             // free old head
        return;
    }

    // Find previous node of the node to be deleted
    for (int i = 0; temp != NULL && i < position - 1; i++)
        temp = temp->next;

    // If position is more than number of nodes
    if (temp == NULL || temp->next == NULL)
        return;

    // Node temp->next is the node to be deleted
    // Store pointer to the next of node to be deleted
    struct Node *next = temp->next->next;

    // Unlink the node from linked list
    free(temp->next); // Free memory

    temp->next = next; // Unlink the deleted node from list
}

void printList(struct Node *node)
{
    while (node != NULL)
    {
        printf("index = 0x%x, tag = 0x%x, way = 0x%x\n", node->index, node->tag, node->way);
        node = node->next;
    }
}

int findPositionInList(struct Node *head, unsigned int index, unsigned int tag) //returns position
{
    struct Node *current = head; // Initialize current
    int count = 0;
    while (current != NULL)
    {
        if (current->index == index && current->tag == tag)
        {
            //printf("Hit\n");
            return count;
        }
        count++;
        current = current->next;
    }
}
