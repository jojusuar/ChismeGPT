//Code adapted from @Leyxargon
// Original source: https://github.com/Leyxargon/c-linked-list

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct node {
	void *n;              /* data field(s) */
	/* float b;
	 * char c;
	 * ... etc.
	 */
	struct node *next;  /* pointer to next element */
}Node;

typedef struct list { /* Implemented by José Julio Suárez */
	Node *head;
	Node *tail;
	int length;
}List;

List *newList(); /* Implemented by José Julio Suárez */

Node *newNode( void * ); /* physically creates a new node */
/* N.B. this function is called by other functions because does not take care
 * of inserting the Node in the list, but delegates this operation to other
 * functions, such as *Insert functions */

void headInsert(List *, void * ); /* Implemented by José Julio Suárez */

void tailInsert(List *, void * ); /* Implemented by José Julio Suárez */

// Node *findNode(Node *, pthread_t ); /* find a node in the list */

// Node *deleteNode(Node *, pthread_t ); /* deletes a node corresponding to the inserted key */

void *pop(List *, int ); /* Implemented by José Julio Suárez */

Node *extractNode(List * , int ); /* Implemented by José Julio Suárez */

void appendNode(List * , Node * ); /* Implemented by José Julio Suárez */

void appendList(List * , List * ); /* Implemented by José Julio Suárez */

void deleteList(List *); /* Modified by José Julio Suárez */

void printList(List *, char * ); /* Implemented by José Julio Suárez */

// void printNodes(Node *); /* prints all the nodes in the list */

void Split(List *, List **); /* split the nodes of the list into two sublists */ /* Modified by José Julio Suárez */