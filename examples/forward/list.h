#define list_for_each(node, head)			\
	for(node = (head)->next; node != (head);	\
	node = node->next)

#define list_for_each_safe(node, next, head)				\ 
	for(node = (head)->next, next = node->next; node != (head);	\
	node = next, next = node->next)

#define list_entry(ptr, type, member)	\
	container_of(ptr, type, member)

struct list_node {
	struct list_head *next, *prev;
};

