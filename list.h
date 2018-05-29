

typedef struct node {
    struct node *next;
    struct node *prev;
    void* dataptr;
	void* data2ptr;
} node;

typedef struct list {
    node *head, *tail, *array;
    int dim;
} list;

void list_init(list *myroot,node* arr, int n);
void list_put(list *myroot, void* dataptr,void* data2ptr, int n);
// node *list_get(list *myroot);
void list_remove(list *myroot, int n);
void list_pop(list *myroot);
unsigned char list_empty(list *myroot);
void* list_popvalue2ptr(list *myroot);
void* list_getvalueptr(list *myroot);
void list_stamp(list *myroot);
unsigned char checklist(list * myroot, int n);
