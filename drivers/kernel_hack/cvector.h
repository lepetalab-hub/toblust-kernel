#ifndef KHACK_CVECTOR_H
#define KHACK_CVECTOR_H

#include <linux/slab.h>
#include <linux/string.h>

#define CVESUCCESS  0
#define CVEFAIL    (-1)

typedef struct {
	void   *data;
	size_t  elem_size;
	size_t  length;
	size_t  capacity;
} *cvector;

static inline cvector cvector_create(size_t elem_size)
{
	cvector v = kmalloc(sizeof(*v), GFP_KERNEL);
	if (!v) return NULL;
	v->data      = NULL;
	v->elem_size = elem_size;
	v->length    = 0;
	v->capacity  = 0;
	return v;
}

static inline void cvector_destroy(cvector v)
{
	if (!v) return;
	kfree(v->data);
	kfree(v);
}

static inline int cvector_pushback(cvector v, const void *elem)
{
	if (!v || !elem) return CVEFAIL;
	if (v->length >= v->capacity) {
		size_t new_cap  = v->capacity ? v->capacity * 2 : 8;
		void  *new_data = krealloc(v->data, new_cap * v->elem_size,
					   GFP_ATOMIC);
		if (!new_data) return CVEFAIL;
		v->data     = new_data;
		v->capacity = new_cap;
	}
	memcpy((char *)v->data + v->length * v->elem_size, elem, v->elem_size);
	v->length++;
	return CVESUCCESS;
}

static inline size_t cvector_length(cvector v)
{
	return v ? v->length : 0;
}

static inline void cvector_val_at(cvector v, size_t idx, void *out)
{
	if (!v || !out || idx >= v->length) return;
	memcpy(out, (char *)v->data + idx * v->elem_size, v->elem_size);
}

#endif /* KHACK_CVECTOR_H */
