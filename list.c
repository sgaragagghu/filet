#include "basic.h"
//Spiego brevemente il funzionamento di questa lista
//La lista e associata ad un array. La dimensione dell array corrisponde alla dimesione massima della lista.
//E fatta ad-hoc per il compito che deve fare nel programma, il put aggiunge l elemento in coda e il pop lo rimuove dalla testa.
//L array serve a rimuovere un elemento preciso senza dover cercarlo, questa e la differenza principale.

void list_init(list * myroot,node* arr,int n)
{
	myroot->head = NULL;
	myroot->tail = NULL;
	myroot->dim = n;
	myroot->array = arr;
}

void list_put(list * myroot, void* valueptr,void* value2ptr, int n)
{
	if(VERBOSELIST==1){
		printf("prima put\n");
		list_stamp(myroot);
	}

	node *mynode = &myroot->array[n];
	//errore particolare... sto cercando di aggiungere nella lista un elemento, ma nell array risulta che in quel particolare indice dell array ci sia gia un altro elemento e che non sia impostato tutto a NULL...
	if(mynode->dataptr!=NULL && mynode->data2ptr!=NULL){
		perror("errore in list put");
		exit(EXIT_FAILURE);
	}
	mynode->dataptr = valueptr;
	mynode->data2ptr = value2ptr;
	mynode->next = NULL;
	if (myroot->tail != NULL) {
		mynode->prev = myroot->tail;
		myroot->tail->next = mynode;
	}
	myroot->tail = mynode;
	if (myroot->head == NULL) {
		mynode->prev = NULL;
		myroot->head = mynode;
	}
	if(VERBOSELIST==1){
		printf("dopo put\n");
		list_stamp(myroot);	
	}
}

void list_remove(list * myroot, int n)
{
	if(VERBOSELIST==1){
		printf("prima remove\n");
		list_stamp(myroot);
	}
	node *mynode = &myroot->array[n];
	if(mynode->prev!=NULL){
		mynode->prev->next = mynode->next;
	}
	else{
		myroot->head=mynode->next;
	}
	if(mynode->next!=NULL){
		mynode->next->prev = mynode->prev;
	}
	else{
		myroot->tail=mynode->prev;
	}
	mynode->next = NULL;
	mynode->prev = NULL;
	mynode->dataptr = NULL;
	mynode->data2ptr = NULL;
	if(myroot->head!=NULL && myroot->head->dataptr==NULL){
		printf("errore rilevato in remove\n");
	}
	if(VERBOSELIST==1){
		printf("dopo remove\n");
		list_stamp(myroot);
	}

}

unsigned char checklist(list * myroot, int n){
	node *mynode = &myroot->array[n];
	if(mynode->dataptr==NULL || mynode->data2ptr==NULL)
		return 0;
	return 1;
}

void* list_getvalueptr(list *myroot){
	return 	myroot->head->dataptr;
}


void* list_popvalue2ptr(list *myroot){
	void* ptr = myroot->head->data2ptr;
	list_pop(myroot);
	return ptr;
}

void list_pop(list * myroot)
{
	if(VERBOSELIST==1){
		printf("prima pop\n");
		list_stamp(myroot);
	}
	node *mynode = myroot->head;
	if(mynode->next==NULL)
		myroot->tail=NULL;
	else
		mynode->next->prev = NULL;
	mynode->dataptr = NULL;
	mynode->data2ptr = NULL;
	mynode->prev = NULL;
	myroot->head = mynode->next;
	mynode->next = NULL;
	if(myroot->head!=NULL && myroot->head->dataptr==NULL){
		printf("errore rilevato in pop\n");
	}
	if(VERBOSELIST==1){
	printf("dopo pop\n");
	list_stamp(myroot);
	}

}

unsigned char list_empty(list *myroot){
	if(myroot->head==NULL){
	return (unsigned char)1;
	}
	return (unsigned char)0;
}

void list_stamp(list *myroot){
	if(myroot->head!=NULL){
		node* mynode=myroot->head;
		for(;;){
			printf("head:%p,tail:%p,numero:%u,next:%p,prev:%p\n",myroot->head,myroot->tail,(unsigned int)*(unsigned char*)mynode->data2ptr,mynode->next,mynode->prev);
			if(mynode->next!=NULL){
				mynode=mynode->next;
			}
			else
				break;
		}
	}
}
