#ifndef _LINUX_RCULIST_H
#define _LINUX_RCULIST_H

static inline void INIT_LIST_HEAD_RCU(struct list_head *list)
{
	CMM_ACCESS_ONCE(list->next) = list;
	CMM_ACCESS_ONCE(list->prev) = list;
}

#define list_next_rcu(list)	(*((struct list_head __rcu **)(&(list)->next)))

void __list_add_rcu(struct list_head *new,
		    struct list_head *prev, struct list_head *next);

static inline void list_add_rcu(struct list_head *new, struct list_head *head)
{
	__list_add_rcu(new, head, head->next);
}

static inline void list_add_tail_rcu(struct list_head *new,
					struct list_head *head)
{
	__list_add_rcu(new, head->prev, head);
}

static inline void list_del_rcu(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->prev = LIST_POISON2;
}

static inline void hlist_del_init_rcu(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		n->pprev = NULL;
	}
}

static inline void list_replace_rcu(struct list_head *old,
				struct list_head *new)
{
	new->next = old->next;
	new->prev = old->prev;
	rcu_assign_pointer(list_next_rcu(new->prev), new);
	new->next->prev = new;
	old->prev = LIST_POISON2;
}

static inline void list_splice_init_rcu(struct list_head *list,
	struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	if (list_empty(list))
		return;

	/*
	 * "first" and "last" tracking list, so initialize it.  RCU readers
	 * have access to this list, so we must use INIT_LIST_HEAD_RCU()
	 * instead of INIT_LIST_HEAD().
	 */

	INIT_LIST_HEAD_RCU(list);

	/*
	 * At this point, the list body still points to the source list.
	 * Wait for any readers to finish using the list before splicing
	 * the list body into the new list.  Any new readers will see
	 * an empty list.
	 */

	synchronize_rcu();

	/*
	 * Readers are finished with the source list, so perform splice.
	 * The order is important if the new list is global and accessible
	 * to concurrent RCU readers.  Note that RCU readers are not
	 * permitted to traverse the prev pointers without excluding
	 * this function.
	 */

	last->next = at;
	rcu_assign_pointer(list_next_rcu(head), first);
	first->prev = head;
	at->prev = last;
}

#define list_entry_rcu(ptr, type, member) \
({ \
	typeof(*ptr) __rcu *__ptr = (typeof(*ptr) __rcu __force *)ptr; \
	container_of((typeof(ptr))rcu_dereference_raw(__ptr), type, member); \
})

#define list_first_or_null_rcu(ptr, type, member) \
({ \
	struct list_head *__ptr = (ptr); \
	struct list_head *__next = ACCESS_ONCE(__ptr->next); \
	likely(__ptr != __next) ? list_entry_rcu(__next, type, member) : NULL; \
})

#define list_for_each_entry_rcu(pos, head, member) \
	for (pos = list_entry_rcu((head)->next, typeof(*pos), member); \
		&pos->member != (head); \
		pos = list_entry_rcu(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_continue_rcu(pos, head, member) 		\
	for (pos = list_entry_rcu(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head);	\
	     pos = list_entry_rcu(pos->member.next, typeof(*pos), member))

static inline void hlist_del_rcu(struct hlist_node *n)
{
	__hlist_del(n);
	n->pprev = LIST_POISON2;
}

static inline void hlist_replace_rcu(struct hlist_node *old,
					struct hlist_node *new)
{
	struct hlist_node *next = old->next;

	new->next = next;
	new->pprev = old->pprev;
	rcu_assign_pointer(*(struct hlist_node __rcu **)new->pprev, new);
	if (next)
		new->next->pprev = &new->next;
	old->pprev = LIST_POISON2;
}

#define hlist_first_rcu(head)	(*((struct hlist_node __rcu **)(&(head)->first)))
#define hlist_next_rcu(node)	(*((struct hlist_node __rcu **)(&(node)->next)))
#define hlist_pprev_rcu(node)	(*((struct hlist_node __rcu **)((node)->pprev)))

static inline void hlist_add_head_rcu(struct hlist_node *n,
					struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first;
	n->pprev = &h->first;
	rcu_assign_pointer(hlist_first_rcu(h), n);
	if (first)
		first->pprev = &n->next;
}

static inline void hlist_add_before_rcu(struct hlist_node *n,
					struct hlist_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	rcu_assign_pointer(hlist_pprev_rcu(n), n);
	next->pprev = &n->next;
}

static inline void hlist_add_behind_rcu(struct hlist_node *n,
					struct hlist_node *prev)
{
	n->next = prev->next;
	n->pprev = &prev->next;
	rcu_assign_pointer(hlist_next_rcu(prev), n);
	if (n->next)
		n->next->pprev = &n->next;
}

#define __hlist_for_each_rcu(pos, head)				\
	for (pos = rcu_dereference(hlist_first_rcu(head));	\
	     pos;						\
	     pos = rcu_dereference(hlist_next_rcu(pos)))

#define hlist_for_each_entry_rcu(pos, head, member)			\
	for (pos = hlist_entry_safe (rcu_dereference_raw(hlist_first_rcu(head)),\
			typeof(*(pos)), member);			\
		pos;							\
		pos = hlist_entry_safe(rcu_dereference_raw(hlist_next_rcu(\
			&(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_rcu_notrace(pos, head, member)			\
	for (pos = hlist_entry_safe (rcu_dereference_raw_notrace(hlist_first_rcu(head)),\
			typeof(*(pos)), member);			\
		pos;							\
		pos = hlist_entry_safe(rcu_dereference_raw_notrace(hlist_next_rcu(\
			&(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_rcu_bh(pos, head, member)			\
	for (pos = hlist_entry_safe(rcu_dereference_bh(hlist_first_rcu(head)),\
			typeof(*(pos)), member);			\
		pos;							\
		pos = hlist_entry_safe(rcu_dereference_bh(hlist_next_rcu(\
			&(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_continue_rcu(pos, member)			\
	for (pos = hlist_entry_safe(rcu_dereference_raw(hlist_next_rcu( \
			&(pos)->member)), typeof(*(pos)), member);	\
	     pos;							\
	     pos = hlist_entry_safe(rcu_dereference_raw(hlist_next_rcu(	\
			&(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_continue_rcu_bh(pos, member)		\
	for (pos = hlist_entry_safe(rcu_dereference_bh(hlist_next_rcu(  \
			&(pos)->member)), typeof(*(pos)), member);	\
	     pos;							\
	     pos = hlist_entry_safe(rcu_dereference_bh(hlist_next_rcu(	\
			&(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_from_rcu(pos, member)			\
	for (; pos;							\
	     pos = hlist_entry_safe(rcu_dereference((pos)->member.next),\
			typeof(*(pos)), member))

#endif
