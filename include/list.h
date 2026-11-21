#ifndef LIST_H
#define LIST_H
#include <stddef.h>
#define container_of(ptr, type, member) ({        \
 const typeof(((type*)0)->member) *__mptr=(ptr);  \
 (type*)((char*)__mptr - offsetof(type, member)); \
})

#define list_entry(p, type, member) container_of(p, type, member)
#define list_for_each_entry(pos, head, member)                         \
 for (pos = list_entry((head)->next, typeof(*pos), member);            \
  &pos->member != (head);                                              \
  pos = list_entry(pos->member.next, typeof(*pos), member))



 struct list_head {
 struct list_head *prev, *next;
};

static inline void list_init(struct list_head *list) {
 list->next = list->prev = list;
}
static inline int list_empty(struct list_head *list) {
 return list->next == list;
}
static inline void list_add_tail(struct list_head *n, struct list_head *h) {
 n->prev = h->prev; n->next = h;
 h->prev->next = n; h->prev = n;
}
static inline void list_del(struct list_head *n) {
 n->prev->next = n->next; n->next->prev = n->prev;
 n->prev = n->next = n;
}
#endif
