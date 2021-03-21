#ifndef _LP_LIST_H
#define _LP_LIST_H

#define lp_offset_of(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)

#define lp_container_of(ptr, type, member)                                                         \
    ({                                                                                             \
        const typeof(((type *)0)->member) *__mptr = (ptr);                                         \
        (type *)((char *)__mptr - lp_offset_of(type, member));                                     \
    })

static inline void lp_prefetch(const void *x) { ; }
static inline void lp_prefetchw(const void *x) { ; }

#define LP_LIST_POISON1 ((void *)0x00100100)
#define LP_LIST_POISON2 ((void *)0x00200200)

struct lp_list_head {
    struct lp_list_head *next, *prev;
};

#define LP_LIST_HEAD_INIT(name)                                                                    \
    {                                                                                              \
        &(name), &(name)                                                                           \
    }

#define LP_LIST_HEAD(name) struct lp_list_head name = LP_LIST_HEAD_INIT(name)

#define LP_INIT_LIST_HEAD(ptr)                                                                     \
    do {                                                                                           \
        (ptr)->next = (ptr);                                                                       \
        (ptr)->prev = (ptr);                                                                       \
    } while (0)

/*
* Insert a new entry between two known consecutive entries.
*
* This is only for internal list manipulation where we know
* the prev/next entries already!
*/
static inline void __lp_list_add(struct lp_list_head *new, struct lp_list_head *prev,
                                 struct lp_list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/**
* lp_list_add - add a new entry
* @new: new entry to be added
* @head: list head to add it after
*
* Insert a new entry after the specified head.
* This is good for implementing stacks.
*/
static inline void lp_list_add(struct lp_list_head *new, struct lp_list_head *head)
{
    __lp_list_add(new, head, head->next);
}

/**
* lp_list_add_tail - add a new entry
* @new: new entry to be added
* @head: list head to add it before
*
* Insert a new entry before the specified head.
* This is useful for implementing queues.
*/
static inline void lp_list_add_tail(struct lp_list_head *new, struct lp_list_head *head)
{
    __lp_list_add(new, head->prev, head);
}

static inline void __lp_list_del(struct lp_list_head *prev, struct lp_list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void lp_list_del(struct lp_list_head *entry)
{
    if (entry->next != LP_LIST_POISON1 && entry->prev != LP_LIST_POISON2)
        __lp_list_del(entry->prev, entry->next);
    entry->next = LP_LIST_POISON1;
    entry->prev = LP_LIST_POISON2;
}

static inline void lp_list_del_init(struct lp_list_head *entry)
{
    __lp_list_del(entry->prev, entry->next);
    LP_INIT_LIST_HEAD(entry);
}

static inline void lp_list_move(struct lp_list_head *list, struct lp_list_head *head)
{
    __lp_list_del(list->prev, list->next);
    lp_list_add(list, head);
}

static inline void lp_list_move_tail(struct lp_list_head *list, struct lp_list_head *head)
{
    __lp_list_del(list->prev, list->next);
    lp_list_add_tail(list, head);
}

static inline int lp_list_empty(const struct lp_list_head *head) { return head->next == head; }

static inline int lp_list_number(const struct lp_list_head *head)
{
    int count = 0;
    struct lp_list_head *next = (head)->next;
    while (next != (head)) {
        next = next->next;
        count++;
    }
    return count;
}

static inline int lp_list_empty_careful(const struct lp_list_head *head)
{
    struct lp_list_head *next = head->next;
    return (next == head) && (next == head->prev);
}

static inline void __lp_list_splice(struct lp_list_head *list, struct lp_list_head *head)
{
    struct lp_list_head *first = list->next;
    struct lp_list_head *last = list->prev;
    struct lp_list_head *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}

/**
* lp_list_splice - join two lists
* @list: the new list to add.
* @head: the place to add it in the first list.
*/
static inline void lp_list_splice(struct lp_list_head *list, struct lp_list_head *head)
{
    if (!lp_list_empty(list))
        __lp_list_splice(list, head);
}

/**
* lp_list_splice_init - join two lists and reinitialise the emptied list.
* @list: the new list to add.
* @head: the place to add it in the first list.
*
* The list at @list is reinitialised
*/
static inline void lp_list_splice_init(struct lp_list_head *list, struct lp_list_head *head)
{
    if (!lp_list_empty(list)) {
        __lp_list_splice(list, head);
        LP_INIT_LIST_HEAD(list);
    }
}

#define lp_list_entry(ptr, type, member) lp_container_of(ptr, type, member)

#define lp_list_for_each(pos, head)                                                                \
    for (pos = (head)->next; lp_prefetch(pos->next), pos != (head); pos = pos->next)

#define __lp_list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)

#define lp_list_for_each_prev(pos, head)                                                           \
    for (pos = (head)->prev; lp_prefetch(pos->prev), pos != (head); pos = pos->prev)

#define lp_list_for_each_safe(pos, n, head)                                                        \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define lp_list_for_each_entry(pos, head, member)                                                  \
    for (pos = lp_list_entry((head)->next, typeof(*pos), member);                                  \
         lp_prefetch(pos->member.next), &pos->member != (head);                                    \
         pos = lp_list_entry(pos->member.next, typeof(*pos), member))

#define lp_list_for_each_entry_reverse(pos, head, member)                                          \
    for (pos = lp_list_entry((head)->prev, typeof(*pos), member);                                  \
         lp_prefetch(pos->member.prev), &pos->member != (head);                                    \
         pos = lp_list_entry(pos->member.prev, typeof(*pos), member))

#define lp_list_prepare_entry(pos, head, member)                                                   \
    ((pos) ?: lp_list_entry(head, typeof(*pos), member))

#define lp_list_for_each_entry_continue(pos, head, member)                                         \
    for (pos = lp_list_entry(pos->member.next, typeof(*pos), member);                              \
         lp_prefetch(pos->member.next), &pos->member != (head);                                    \
         pos = lp_list_entry(pos->member.next, typeof(*pos), member))

#define lp_list_for_each_entry_safe(pos, n, head, member)                                          \
    for (pos = lp_list_entry((head)->next, typeof(*pos), member),                                  \
        n = lp_list_entry(pos->member.next, typeof(*pos), member);                                 \
         &pos->member != (head); pos = n, n = lp_list_entry(n->member.next, typeof(*n), member))

// HASH LIST
struct lp_hlist_head {
    struct lp_hlist_node *first;
};

struct lp_hlist_node {
    struct lp_hlist_node *next, **pprev;
};

#define LP_HLIST_HEAD_INIT                                                                         \
    {                                                                                              \
        .first = NULL                                                                              \
    }
#define LP_HLIST_HEAD(name) struct lp_hlist_head name = {.first = NULL}
#define LP_INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
#define LP_INIT_HLIST_NODE(ptr) ((ptr)->next = NULL, (ptr)->pprev = NULL)

static inline int lp_hlist_unhashed(const struct lp_hlist_node *h) { return !h->pprev; }

static inline int lp_hlist_empty(const struct lp_hlist_head *h) { return !h->first; }

static inline void __lp_hlist_del(struct lp_hlist_node *n)
{
    struct lp_hlist_node *next = n->next;
    struct lp_hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

static inline void lp_hlist_del(struct lp_hlist_node *n)
{
    __lp_hlist_del(n);
    n->next = LP_LIST_POISON1;
    n->pprev = LP_LIST_POISON2;
}

static inline void lp_hlist_del_init(struct lp_hlist_node *n)
{
    if (n->pprev) {
        __lp_hlist_del(n);
        LP_INIT_HLIST_NODE(n);
    }
}

static inline void lp_hlist_add_head(struct lp_hlist_node *n, struct lp_hlist_head *h)
{
    struct lp_hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/* next must be != NULL */
static inline void lp_hlist_add_before(struct lp_hlist_node *n, struct lp_hlist_node *next)
{
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

static inline void lp_hlist_add_after(struct lp_hlist_node *n, struct lp_hlist_node *next)
{
    next->next = n->next;
    n->next = next;
    next->pprev = &n->next;

    if (next->next)
        next->next->pprev = &next->next;
}

#define lp_hlist_entry(ptr, type, member) lp_container_of(ptr, type, member)

#define lp_hlist_for_each(pos, head)                                                               \
    for (pos = (head)->first; pos && ({                                                            \
                                  lp_prefetch(pos->next);                                          \
                                  1;                                                               \
                              });                                                                  \
         pos = pos->next)

#define lp_hlist_for_each_safe(pos, n, head)                                                       \
    for (pos = (head)->first; pos && ({                                                            \
                                  n = pos->next;                                                   \
                                  1;                                                               \
                              });                                                                  \
         pos = n)

#define lp_hlist_for_each_entry(tpos, pos, head, member)                                           \
    for (pos = (head)->first; pos && ({                                                            \
                                  lp_prefetch(pos->next);                                          \
                                  1;                                                               \
                              }) &&                                                                \
                              ({                                                                   \
                                  tpos = lp_hlist_entry(pos, typeof(*tpos), member);               \
                                  1;                                                               \
                              });                                                                  \
         pos = pos->next)

#define lp_hlist_for_each_entry_continue(tpos, pos, member)                                        \
    for (pos = (pos)->next; pos && ({                                                              \
                                lp_prefetch(pos->next);                                            \
                                1;                                                                 \
                            }) &&                                                                  \
                            ({                                                                     \
                                tpos = lp_hlist_entry(pos, typeof(*tpos), member);                 \
                                1;                                                                 \
                            });                                                                    \
         pos = pos->next)

#define lp_hlist_for_each_entry_from(tpos, pos, member)                                            \
    for (; pos && ({                                                                               \
               lp_prefetch(pos->next);                                                             \
               1;                                                                                  \
           }) &&                                                                                   \
           ({                                                                                      \
               tpos = lp_hlist_entry(pos, typeof(*tpos), member);                                  \
               1;                                                                                  \
           });                                                                                     \
         pos = pos->next)

#define lp_hlist_for_each_entry_safe(tpos, pos, n, head, member)                                   \
    for (pos = (head)->first; pos && ({                                                            \
                                  n = pos->next;                                                   \
                                  1;                                                               \
                              }) &&                                                                \
                              ({                                                                   \
                                  tpos = lp_hlist_entry(pos, typeof(*tpos), member);               \
                                  1;                                                               \
                              });                                                                  \
         pos = n)

#endif
