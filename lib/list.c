/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"

#include "inn/list.h"

void
list_new(struct list *list)
{
    list->head = (struct node *)&list->tail;
    list->tailpred = (struct node *)&list->head;
    list->tail = NULL;
}

struct node *
list_addhead(struct list *list, struct node *node)
{
    node->succ = list->head;
    node->pred = (struct node *)&list->head;
    list->head->pred = node;
    list->head = node;
    return node;
}

struct node *
list_addtail(struct list *list, struct node *node)
{
    node->succ = (struct node *)&list->tail;
    node->pred = list->tailpred;
    list->tailpred->succ = node;
    list->tailpred = node;
    return node;
}

struct node *
list_remhead(struct list *list)
{
    struct node *node;

    node = list->head->succ;
    if (node) {
	node->pred = (struct node *)&list->head;
        node = list->head;
        list->head = node->succ;
    }
    return node;
}

struct node *
list_head(struct list *list)
{
    if (list->head->succ)
	return list->head;
    return NULL;
}

struct node *
list_tail(struct list *list)
{
    if (list->tailpred->pred)
	return list->tailpred;
    return NULL;
}

struct node *
list_succ(struct node *node)
{
    if (node->succ->succ)
	return node->succ;
    return NULL;
}

struct node *
list_pred(struct node *node)
{
    if (node->pred->pred)
	return node->pred;
    return NULL;
}

struct node *
list_remove(struct node *node)
{
    node->pred->succ = node->succ;
    node->succ->pred = node->pred;
    return node;
}

struct node *
list_remtail(struct list *list)
{
    struct node *node;

    node = list_tail(list);
    if (node)
	list_remove(node);
    return node;
}

bool
list_isempty(struct list *list)
{
    return list->tailpred == (struct node *)list;
}

struct node *
list_insert(struct list *list, struct node *node, struct node *pred)
{
    if (pred) {
	if (pred->succ) {
	    node->succ = pred->succ;
	    node->pred = pred;
	    pred->succ->pred = node;
	    pred->succ = node;
	} else {
	    list_addtail(list, node);
	}
    } else {
	list_addhead(list, node);
    }
    return node;
}
