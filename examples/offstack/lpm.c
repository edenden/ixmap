#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stddef.h>

#include "main.h"
#include "lpm.h"

static void lpm_init_node(struct lpm_node *node);
static unsigned int lpm_index(void *prefix, unsigned int offset,
	unsigned int range);
static struct list_head *_lpm_lookup(void *dst,
	struct lpm_node *parent, unsigned int offset);
static int _lpm_add(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id,
	void *ptr, struct ixmap_desc *desc,
	struct lpm_node *parent, unsigned int offset);
static int _lpm_delete(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id,
	struct lpm_node *parent, unsigned int offset);
static void _lpm_delete_all(struct lpm_table *table,
	struct lpm_node *parent);
static int _lpm_traverse(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, struct lpm_node *parent,
	unsigned int offset);
static int lpm_entry_insert(struct lpm_table *table, struct list_head *head,
	unsigned int id, unsigned int prefix_len, struct list_head *list);
static int lpm_entry_delete(struct lpm_table *table, struct list_head *head,
	unsigned int id, unsigned int prefix_len);
static void lpm_entry_delete_all(struct lpm_table *table, struct list_head *head);

void lpm_init(struct lpm_table *table)
{
	struct lpm_node *node;
	int i;

	for(i = 0; i < TABLE_SIZE_16; i++){
		node = &table->node[i];
		lpm_init_node(node);
	}

	return;
}

static void lpm_init_node(struct lpm_node *node)
{
	node->next_table = NULL;
	INIT_LIST_HEAD(&node->head);
	node->area = NULL;

	return;
}

static unsigned int lpm_index(void *prefix, unsigned int offset,
	unsigned int range)
{
	unsigned int shift, word, mask;

	shift = 32 - ((offset % 32) + range);
        word = offset >> 5;
        mask = (1 << range) - 1;

        return (ntohl(((uint32_t *)prefix)[word]) >> shift) & mask;
}

struct lpm_entry *lpm_lookup(struct lpm_table *table,
	void *dst)
{
	unsigned int index;
	struct lpm_node *node;
	struct list_head *head;
	struct lpm_entry *entry;

	index = lpm_index(dst, 0, 16);
	node = &table->node[index];

	if(node->next_table){
		head = _lpm_lookup(dst, node, 16);
	}else{
		head = &node->head;
	}

	entry = list_first_entry_or_null(head, struct lpm_entry, list);
	return entry;
}

static struct list_head *_lpm_lookup(void *dst,
	struct lpm_node *parent, unsigned int offset)
{
	unsigned int index;
	struct lpm_node *node;
	struct list_head *head;

	index = lpm_index(dst, offset, 8);
	node = &parent->next_table[index];

	if(node->next_table){
		head = _lpm_lookup(dst, node, offset + 8);
	}else{
		head = &node->head;
	}

	return head;
}

int lpm_add(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id,
	void *ptr, struct ixmap_desc *desc)
{
	unsigned int index;
	struct lpm_node *node;
	struct lpm_entry *entry;
	unsigned int range, mask;
	struct ixmap_marea *area;
	int i, ret, entry_allocated = 0;

	index = lpm_index(prefix, 0, 16);

	if(prefix_len > 16){
		node = &table->node[index];
		ret = _lpm_add(table, prefix, prefix_len, id,
			ptr, desc, node, 16);
		if(ret < 0)
			goto err_lpm_add;
	}else{
		range = 1 << (16 - prefix_len);
		mask = ~(range - 1);
		index &= mask;

		for(i = 0; i < range; i++, entry_allocated++){
			node = &table->node[index | i];

			area = ixmap_mem_alloc(desc, sizeof(struct lpm_entry));
			if(!area)
				goto err_lpm_add_self;

			entry = area->ptr;
			entry->area = area;
			entry->ptr = ptr;

			ret = lpm_entry_insert(table, &node->head, id,
				prefix_len, &entry->list);
			if(ret < 0)
				goto err_entry_insert;

			continue;
err_entry_insert:
			ixmap_mem_free(area);
			goto err_lpm_add_self;
		}
	}

	return 0;

err_lpm_add_self:
	for(i = 0; i < entry_allocated; i++){
		node = &table->node[index | i];
		lpm_entry_delete(table, &node->head, id, prefix_len);
	}
err_lpm_add:
	return -1;
}

static int _lpm_add(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id,
	void *ptr, struct ixmap_desc *desc,
	struct lpm_node *parent, unsigned int offset)
{
	struct lpm_node *node;
	struct lpm_entry *entry;
	unsigned int index;
	unsigned int range, mask;
	struct ixmap_marea *area;
	int i, ret, entry_allocated = 0;

	if(!parent->next_table){
		area = ixmap_mem_alloc(desc,
			sizeof(struct lpm_node) * TABLE_SIZE_8);
		if(!area)
			goto err_table_alloc;

		parent->next_table = area->ptr;
		parent->area = area;

		for(i = 0; i < TABLE_SIZE_8; i++){
			node = &parent->next_table[i];
			lpm_init_node(node);
		}
	}

	index = lpm_index(prefix, offset, 8);

	if(prefix_len - offset > 8){
		node = &parent->next_table[index];
		ret = _lpm_add(table, prefix, prefix_len, id,
			ptr, desc, node, offset + 8);
		if(ret < 0)
			goto err_lpm_add;
	}else{
		range = 1 << (8 - (prefix_len - offset));
		mask = ~(range - 1);
		index &= mask;

		for(i = 0; i < range; i++){
			node = &parent->next_table[index | i];

			area = ixmap_mem_alloc(desc, sizeof(struct lpm_entry));
			if(!area)
				goto err_lpm_add_self;

			entry = area->ptr;
			entry->area = area;
			entry->ptr = ptr;

			ret = lpm_entry_insert(table, &node->head, id,
				prefix_len, &entry->list);
			if(ret < 0)
				goto err_entry_insert;

			continue;
err_entry_insert:
			ixmap_mem_free(area);
			goto err_lpm_add_self;
		}
	}

	return 0;

err_lpm_add_self:
	for(i = 0; i < entry_allocated; i++){
		node = &parent->next_table[index | i];
		lpm_entry_delete(table, &node->head, id, prefix_len);
	}
err_lpm_add:
	for(i = 0; i < TABLE_SIZE_8; i++){
		node = &parent->next_table[i];
		if(node->next_table || !list_empty(&node->head)){
			goto err_table_alloc;
		}
	}
	ixmap_mem_free(parent->area);
	parent->next_table = NULL;
err_table_alloc:
        return -1;
}

int lpm_delete(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id)
{
	unsigned int index;
	struct lpm_node *node;
	unsigned int range, mask;
	int i, ret;

	index = lpm_index(prefix, 0, 16);

	if(prefix_len > 16){
		node = &table->node[index];
		ret = _lpm_delete(table, prefix, prefix_len, id, node, 16);
		if(ret < 0)
			goto err_delete;
	}else{
		range = 1 << (16 - prefix_len);
		mask = ~(range - 1);
		index &= mask;

		for(i = 0; i < range; i++){
			node = &table->node[index | i];
			ret = lpm_entry_delete(table, &node->head, id, prefix_len);
			if(ret < 0)
				goto err_delete;
		}
	}

	return 0;

err_delete:
	return -1;
}

static int _lpm_delete(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, unsigned int id,
	struct lpm_node *parent, unsigned int offset)
{
	struct lpm_node *node;
	unsigned int index;
	unsigned int range, mask;
	int i, ret;

	if(!parent->next_table)
		goto err_delete;

	index = lpm_index(prefix, offset, 8);

	if(prefix_len - offset > 8){
		node = &parent->next_table[index];
		ret = _lpm_delete(table, prefix, prefix_len, id, node, offset + 8);
		if(ret < 0)
			goto err_delete;
	}else{
		range = 1 << (8 - (prefix_len - offset));
		mask = ~(range - 1);
		index &= mask;

		for(i = 0; i < range; i++){
			node = &parent->next_table[index | i];
			ret = lpm_entry_delete(table, &node->head, id, prefix_len);
			if(ret < 0)
				goto err_delete;
		}
	}

	for(i = 0; i < TABLE_SIZE_8; i++){
		node = &parent->next_table[i];
		if(node->next_table || !list_empty(&node->head)){
			goto out;
		}
	}

	ixmap_mem_free(parent->area);
	parent->next_table = NULL;

out:
	return 0;
err_delete:
	return -1;
}

void lpm_delete_all(struct lpm_table *table)
{
	struct lpm_node *node;
	int i;

	for(i = 0; i < TABLE_SIZE_16; i++){
		node = &table->node[i];
		_lpm_delete_all(table, node);
		if(!list_empty(&node->head)){
			lpm_entry_delete_all(table, &node->head);
		}
	}

	return;
}

static void _lpm_delete_all(struct lpm_table *table,
	struct lpm_node *parent)
{
	struct lpm_node *node;
	int i;

	if(!parent->next_table)
		goto out;

	for(i = 0; i < TABLE_SIZE_8; i++){
		node = &parent->next_table[i];
		_lpm_delete_all(table, node);
		if(!list_empty(&node->head)){
			lpm_entry_delete_all(table, &node->head);
		}
	}

	ixmap_mem_free(parent->area);
	parent->next_table = NULL;

out:
	return;
}

int lpm_traverse(struct lpm_table *table, void *prefix,
	unsigned int prefix_len)
{
	unsigned int index;
	struct lpm_node *node;
	unsigned int range, mask;
	int i;

	index = lpm_index(prefix, 0, 16);

	if(prefix_len > 16){
		node = &table->node[index];
		_lpm_traverse(table, prefix, prefix_len, node, 16);
	}else{
		range = 1 << (16 - prefix_len);
		mask = ~((1 << range) - 1);
		index &= mask;

		for(i = 0; i < range; i++){
			node = &table->node[index | i];
			if(!list_empty(&node->head)){
				table->entry_dump(&node->head);
			}
		}
	}

	return 0;
}

static int _lpm_traverse(struct lpm_table *table, void *prefix,
	unsigned int prefix_len, struct lpm_node *parent,
	unsigned int offset)
{
	struct lpm_node *node;
	unsigned int index;
	unsigned int range, mask;
	int i;

	if(!parent->next_table)
		goto err_traverse;

	index = lpm_index(prefix, offset, 8);

	if(prefix_len - offset > 8){
		node = &parent->next_table[index];
		_lpm_traverse(table, prefix, prefix_len, node, offset + 8);
	}else{
		range = 1 << (8 - (prefix_len - offset));
		mask = ~((1 << range) - 1);
		index &= mask;

		for(i = 0; i < range; i++){
			node = &parent->next_table[index | i];
			if(!list_empty(&node->head)){
				table->entry_dump(&node->head);
			}
		}
	}

	return 0;

err_traverse:
	return -1;
}

static int lpm_entry_insert(struct lpm_table *table, struct list_head *head,
	unsigned int id, unsigned int prefix_len, struct list_head *list)
{
	struct lpm_entry *entry_lpm;
	struct list_head *prior;

	prior = NULL;

	list_for_each_entry(entry_lpm, head, list){
		if(!table->entry_identify(entry_lpm->ptr, id, prefix_len)){
			goto err_entry_exist;
		}

		if(table->entry_compare(entry_lpm->ptr, prefix_len)){
			prior = &entry_lpm->list;
		}
	}

	entry_lpm = list_entry(list, struct lpm_entry, list);
	table->entry_pull(entry_lpm->ptr);
	list_add(list, prior ? prior : head);

	return 0;

err_entry_exist:
	return -1;
}

static int lpm_entry_delete(struct lpm_table *table, struct list_head *head,
	unsigned int id, unsigned int prefix_len)
{
	struct lpm_entry *entry_lpm, *entry_n;

	list_for_each_entry_safe(entry_lpm, entry_n, head, list){
		if(!table->entry_identify(entry_lpm->ptr, id, prefix_len)){
			list_del(&entry_lpm->list);
			table->entry_put(entry_lpm->ptr);
			ixmap_mem_free(entry_lpm->area);

			return 0;
		}
	}

	return -1;
}

static void lpm_entry_delete_all(struct lpm_table *table, struct list_head *head)
{
	struct lpm_entry *entry_lpm, *entry_n;

	list_for_each_entry_safe(entry_lpm, entry_n, head, list){
		list_del(&entry_lpm->list);
		table->entry_put(entry_lpm->ptr);
		ixmap_mem_free(entry_lpm->area);
	}

	return;
}

