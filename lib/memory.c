#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <net/ethernet.h>

#include "ixmap.h"
#include "memory.h"

static struct ixmap_mnode *ixmap_mnode_alloc(struct ixmap_mnode *parent,
	void *ptr, unsigned int size, unsigned int index);
static void ixmap_mnode_release(struct ixmap_mnode *node);
static void _ixmap_mem_destroy(struct ixmap_mnode *node);
static struct ixmap_marea *_ixmap_mem_alloc(struct ixmap_mnode *node,
	unsigned int size);
static void _ixmap_mem_free(struct ixmap_mnode *node);

static struct ixmap_mnode *ixmap_mnode_alloc(struct ixmap_mnode *parent,
	void *ptr, unsigned int size, unsigned int index)
{
	struct ixmap_mnode *node;

	node = malloc(sizeof(struct ixmap_mnode));
	if(!node)
		goto err_alloc_node;

	node->parent	= parent;
	node->child[0]	= NULL;
	node->child[1]	= NULL;
	node->allocated	= 0;
	node->index	= index;
	node->size	= size;
	node->area.ptr	= ptr;

	return node;

err_alloc_node:
	return NULL;
}

static void ixmap_mnode_release(struct ixmap_mnode *node)
{
	struct ixmap_mnode *parent;

	parent = node->parent;
	if(parent)
		parent->child[node->index] = NULL;

	free(node);
	return;
}

struct ixmap_mnode *ixmap_mem_init(void *ptr, unsigned int size)
{
	struct ixmap_mnode *root;

	root = ixmap_mnode_alloc(NULL, ptr, size, 0);
	if(!root)
		goto err_alloc_root;

	return root;

err_alloc_root:
	return NULL;
}

void ixmap_mem_destroy(struct ixmap_mnode *node)
{
	_ixmap_mem_destroy(node);
}

static void _ixmap_mem_destroy(struct ixmap_mnode *node)
{
	struct ixmap_mnode *child;
	int i;

	for(i = 0; i < 2; i++){
		child = node->child[i];
		if(child)
			_ixmap_mem_destroy(child);
	}

	ixmap_mnode_release(node);
	return;
}

struct ixmap_marea *ixmap_mem_alloc(struct ixmap_desc *desc,
	unsigned int size)
{
	return _ixmap_mem_alloc(desc->node, ALIGN(size, L1_CACHE_BYTES));
}

static struct ixmap_marea *_ixmap_mem_alloc(struct ixmap_mnode *node,
	unsigned int size)
{
	void *ptr_new;
	unsigned int size_new;
	int i, buddy_allocated;
	struct ixmap_marea *ret;

	ret = NULL;

	if(!node)
		goto ign_node;

	if((node->size >> 1) < size){
		if(!node->allocated
		&& node->size >= size){
			ret = &node->area;
		}
	}else{
		if(!node->allocated){
			size_new = node->size >> 1;
			for(i = 0, buddy_allocated = 0; i < 2; i++, buddy_allocated++){
				ptr_new = node->area.ptr + (size_new * i);

				node->child[i] =
					ixmap_mnode_alloc(node, ptr_new, size_new, i);
				if(!node->child[i])
					goto err_alloc_child;
			}
		}

		for(i = 0; i < 2; i++){
			ret = _ixmap_mem_alloc(node->child[i], size);
			if(ret)
				break;
		}
	}

	if(ret)
		node->allocated = 1;

ign_node:
	return ret;

err_alloc_child:
	for(i = 0; i < buddy_allocated; i++)
		ixmap_mnode_release(node->child[i]);
	return NULL;
}

void ixmap_mem_free(struct ixmap_marea *area)
{
	struct ixmap_mnode *node;

	node = container_of(area, struct ixmap_mnode, area);
	_ixmap_mem_free(node);
}

static void _ixmap_mem_free(struct ixmap_mnode *node)
{
	struct ixmap_mnode *parent, *buddy;

	node->allocated = 0;
	
	parent = node->parent;
	if(!parent)
		goto out;

	buddy = parent->child[!node->index];
	if(buddy->allocated)
		goto out;

	ixmap_mnode_release(buddy);
	ixmap_mnode_release(node);
	_ixmap_mem_free(parent);

	return;

out:
	return;
}
