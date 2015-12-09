#ifndef _IXMAP_MEMORY_H
#define _IXMAP_MEMORY_H

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct ixmap_marea {
	void			*ptr;
};

struct ixmap_mnode {
	struct ixmap_mnode	*child[2];
	struct ixmap_mnode	*parent;
	unsigned int		allocated;
	unsigned int		index;
	unsigned int		size;
	struct ixmap_marea	area;
};

struct ixmap_mnode *ixmap_mem_init(void *ptr, unsigned int size, int core_id);
void ixmap_mem_destroy(struct ixmap_mnode *node);

#endif /* _IXMAP_MEMORY_H */
