/* Defines functions for implmenting a dictionary.
 * CSC 357, Assignment 3
 * Given code, Winter '24 */

#include <stdio.h>
#include <stdlib.h>
#include "dict.h"
#include <string.h>

void rehash(Dict *);
/* dcthash: Hashes a string key.
 * NOTE: This is certainly not the best known string hashing function, but it
 *       is reasonably performant and easy to predict when testing. */
static unsigned long int dcthash(char *key) {
    unsigned long int code = 0, i;

    for (i = 0; i < 8 && key[i] != '\0'; i++) {
        code = key[i] + 31 * code;
    }

    return code;
}

/* dctcreate: Creates a new empty dictionary.
 * TODO: Implement this function. It should return a pointer to a newly
 *       dynamically allocated dictionary with an empty backing array. */
Dict *dctcreate() {
	Dict *dict; 
	dict = (Dict *)malloc(sizeof(Dict));
	dict->cap = 30;
        dict->size = 0;
	if (dict == NULL) {
		free(dict);
		return NULL;
	} else {
		dict -> arr = (Node**)calloc(dict->cap, sizeof(Node**));
		if (dict->arr == NULL) {
			free(dict);
			return NULL;
		}	
	}
	return dict;
}


/* dctdestroy: Destroys an existing dictionary.
 * TODO: Implement this function. It should deallocate a dictionary, its
 *       backing array, and all of its nodes. */
void dctdestroy(Dict *dct) {
	Node *curr;
	Node* temp;
	int i = 0;
	if (dct == NULL) {
        	return;
	}
	for (i = 0; i < dct->cap; i++) {
        	curr = dct->arr[i];
        	while (curr != NULL) {
            		temp = curr;
            		curr = curr->next;
            		free(temp->key);
            		free(temp);
        	}
    	}
    free(dct -> arr);
    free(dct);
}


/* dctget: Gets the value to which a key is mapped.
 * TODO: Implement this function. It should return the value to which "key" is
 *       mapped, or NULL if it does not exist. */
void *dctget(Dict *dct, char *key) {
 	unsigned long int idx = 0;   
	Node *curr;
	idx = (dcthash(key))%dct->cap;
	curr = dct->arr[idx];
	while (curr != NULL) {
		if (strcmp(curr->key, key)==0) {
			return (void*)curr->val;
		}
	curr = curr->next;
	}
	return NULL;
}

/* dctinsert: Inserts a key, overwriting any existing value.
 * TODO: Implement this function. It should insert "key" mapped to "val".
 * NOTE: Depending on how you use this dictionary, it may be useful to insert a
 *       dynamically allocated copy of "key", rather than "key" itself. Either
 *       implementation is acceptable, as long as there are no memory leaks. */
void dctinsert(Dict *dct, char *key, void *val) {
	int loadFactor = 0;
        unsigned long int idx = 0;
        Node *curr, *node;
	if (dct == NULL || key == NULL) {
        	return;
    	}
	loadFactor = (dct->size) / (dct -> cap);
	if (loadFactor > 1) {
		(void)rehash(dct);
	}
	
	idx = (dcthash(key))%dct->cap;
	curr = dct->arr[idx];
	
	while (curr != NULL) {
		if (strcmp(curr->key, key) == 0) {
		curr->val = val;
		return;
		}
		curr = curr -> next;
	}
	node = (Node *)malloc(sizeof(Node));
	if (node == NULL) {
		return;
	}
	node->key = (char *)malloc(strlen(key) + 1);
        dct->size = dct->size + 1;
	strcpy(node->key, key);
	node -> val = val;
        node -> next = dct -> arr[idx];
        dct->arr[idx] = node;	
	return;
}

void rehash(Dict *dct){
	Node **newarr = NULL; 	
	int i = 0;
	int newcap = (dct->cap)*2+1;
 	newarr = (Node **)malloc(sizeof(Node *)*newcap);
	memset(newarr, 0, (newcap)*(sizeof(Node*)));
	
	for (i = 0; i < dct->cap; i++) {
		Node *curr = dct->arr[i];
		while (curr != NULL) {
			Node *next = curr->next;
			unsigned long int idx = dcthash(curr->key)%newcap;
            		curr->next = newarr[idx];
			newarr[idx] = curr;
            		curr = next;
		}	
	}
	dct->cap = newcap;
	free(dct->arr);
	dct->arr = newarr;
}


/* dctremove: Removes a key and the value to which it is mapped.
 * TODO: Implement this function. It should remove "key" and return the value
 *       to which it was mapped, or NULL if it did not exist. */
void *dctremove(Dict *dct, char *key) {
	unsigned long int idx = 0;
	int *val = NULL;
	Node *prev = NULL;
	Node *curr = NULL;
    idx = (dcthash(key))%dct->cap;
	curr = dct->arr[idx];

        while (curr != NULL) {
                if (strcmp(curr->key, key)==0) {
                        val = curr -> val;
			val = (void *)val;
			if (prev == NULL) {
				dct->arr[idx] = curr->next;
			} else {
				prev->next = curr->next;
			}
			dct->size = dct->size - 1;
			free(curr->key);
			free(curr);
			return val;
                }
	prev = curr;
        curr = curr->next;
        }
        return NULL;
}

/*dctremoveVAL: Removes a value and the key that mapped to it*/
char *dctremoveVAL(Dict *dct, void* val) {
	if (dct == NULL || val == NULL) {
		return NULL;
	}
	for (int i = 0; i < (dct->cap); i++) {
		Node *curr = dct->arr[i];
        Node *prev = NULL;
	
	while (curr != NULL) {
            if (curr->val == val) {
                char *key = curr->key; // Store key to return

                if (prev == NULL) {
                    dct->arr[i] = curr->next;
                } else {
                    prev->next = curr->next;
                }

                free(curr);
                dct->size--;

                return key;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return NULL; // Value not found

}


/* dctkeys: Enumerates all of the keys in a dictionary.
 * TODO: Implement this function. It should return a dynamically allocated array
 *       of pointers to the keys, in no particular order, or NULL if empty. */
char **dctkeys(Dict *dct) {
	char **arr = (char **)malloc(dct->size * sizeof(char *));
    	int i = 0, k = 0;
	if ((arr == NULL)||(dct->size == 0)) {
    		free(arr);
		return NULL;
	}
	for (i=0; i<dct->cap; i++) {
		Node *curr = dct->arr[i];
		while (curr != NULL) {
			arr[k] = curr->key;
			k++;
			curr = curr ->next;		
		}
	}
	/*arr[k] = NULL;*/
	return arr;
}