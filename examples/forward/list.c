#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

inline void list_init(struct list_node *head)
{
	head->prev = node;
	head->next = node;

	return;
}

inline int list_empty(struct list_node *head)
{
	return head->next == head;
}

inline void list_add(struct list_node *new, struct list_node *prev)
{
	struct list_node *next;

	next = prev->next;

	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;

	return;
}

inline void list_splice(struct list_head *list, struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

inline void list_delete(struct list_head *list)
{
	list->next->prev = list->prev;
	list->prev->next = list->next;
}
 
