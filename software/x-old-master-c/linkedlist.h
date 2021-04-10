/*
 * DDEBUGLIB
 *
 * Linked list macros.
 *
 * In order to use the following macros, you need to define next and prev
 * in struct:
 * struct x { struct x *next; struct x *prev; ... }
 *
 * License: MIT, see COPYING
 * Authors: Antti Partanen <aehparta@iki.fi, duge at IRCnet>
 */

#ifndef _LIBE_LINKEDLIST_H_
#define _LIBE_LINKEDLIST_H_


/**
 * Append new item into list.
 */
#define LL_APP(first, last, item) \
do { \
    item->prev = NULL; \
    item->next = NULL; \
    if (first == NULL) first = item; \
    else { \
        last->next = item; \
        item->prev = last; \
    } \
    last = item; \
} while(0)

/**
 * Insert item into queue using field in struct to determine correct position.
 * field should be integer or similar and the item is inserted after first field
 * that has same or smaller value in list that the item being inserted.
 */
#define LL_INS(first, last, item, loop) \
do { \
    item->prev = NULL; \
    item->next = NULL; \
    loop = last; \
    while (loop) \
    { \
        if (loop->position <= item->position) \
        { \
            if (loop->next) \
            { \
                loop->next->prev = item; \
            } \
            item->next = loop->next; \
            item->prev = loop; \
            loop->next = item; \
            if (last == loop) last = item; \
            break; \
        } \
        loop = loop->prev; \
        if (loop == NULL) \
        { \
            item->next = first; \
            first->prev = item; \
            first = item; \
            break; \
        } \
    } \
    if (last == NULL) \
    { \
        first = last = item; \
    } \
} while(0)

/**
 * Get item from end of the list.
 */
#define LL_POP(first, last, item) \
do { \
    item = NULL; \
    if (last != NULL) \
    { \
        item = last; \
        if (last->prev == NULL) \
        { \
            first = NULL; \
            last = NULL; \
        } \
        else { \
            last = last->prev; \
            last->next = NULL; \
        } \
    } \
} while (0)

/**
 * Get item from top of the list.
 */
#define LL_GET(first, last, item) \
do { \
    item = NULL; \
    if (first != NULL) \
    { \
        item = first; \
        if (first->next == NULL) \
        { \
            first = NULL; \
            last = NULL; \
        } \
        else { \
            first = first->next; \
            first->prev = NULL; \
        } \
    } \
} while (0)

/**
 * Remove given item from list.
 */
#define LL_RM(first, last, item) \
do { \
    if (!item->prev) { \
        first = item->next; \
        if (first) { \
            first->prev = NULL; \
        } \
    } else { \
        item->prev->next = item->next; \
    } \
    if (!item->next) { \
        last = item->prev; \
        if (last) { \
            last->next = NULL; \
        } \
    } else { \
        item->next->prev = item->prev; \
    } \
} while (0)

/**
 * Count items in list.
 */
#define LL_COUNT(first, last, count) \
do { \
    __typeof__(first) _looper; \
    count = 0; \
    _looper = first; \
    while (_looper) { \
        count++; \
        _looper = _looper->next; \
    } \
} while (0)

/**
 * Loop through all items in list, will remove each and execute custom code.
 * Use loop_item inside custom code.
 */
#define LL_GET_LOOP(first, last, custom_code) \
do { \
    __typeof__(first) loop_item; \
    LL_GET(first, last, loop_item); \
    while (loop_item) { \
        do { custom_code; } while (0); \
        LL_GET(first, last, loop_item); \
    } \
} while (0)


#endif /* _LIBE_LINKEDLIST_H_ */

