/*  $Id$
**
*/

#ifndef INN_LIST_H
#define INN_LIST_H 1

#include <inn/defines.h>

struct node {
    struct node *succ;
    struct node *pred;
};

struct list {
    struct node *head;
    struct node *tail;
    struct node *tailpred;
};

BEGIN_DECLS

/* initialise a new list */
void list_new(struct list *list);

/* add a node to the head of the list */
struct node *list_addhead(struct list *list, struct node *node);

/* add a node to the tail of the list */
struct node *list_addtail(struct list *list, struct node *node);

/* return a pointer to the first node on the list */
struct node *list_head(struct list *list);

/* return a pointer to the last node on the list */
struct node *list_tail(struct list *list);

struct node *list_succ(struct node *node);
struct node *list_pred(struct node *node);

struct node *list_remhead(struct list *list);
struct node *list_remove(struct node *node);
struct node *list_remtail(struct list *list);
struct node *list_insert(struct list *list, struct node *node,
			 struct node *pred);

bool list_isempty(struct list *list);

END_DECLS

#endif /* INN_LIST_H */
