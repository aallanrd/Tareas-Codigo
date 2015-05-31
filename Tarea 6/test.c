/* prueba1.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "task.h"
#include "scheduler.h"
#include "taskimpl.h"





//enum {STACK = 32768};

int counter;

void myTask(void *arg) {
  int runs = rand() % 1000;
  int i;
    int **arr = malloc(runs * sizeof(int *));

    if (arr == NULL) {
      printf("Memory failed to allocate!\n");
    }

    for (i = 0; i < runs; i++) {
      arr[i] = malloc(sizeof(int));
      if (arr[i] == NULL) {
        printf("Memory failed to allocate!\n");
      }
      taskyield();
      *(arr[i]) = i+1;
    }

    for (i = 0; i < runs; i++) {
      if (*(arr[i]) != i+1) {
        printf("Memory failed to contain correct data after many allocations!\n");
      }
    }
    for (i = 0; i < runs; i++)
      free(arr[i]);
  
    free(arr);
    printf("Memory was allocated, used, and freed!\n");
}

void taskadn(void *arg) {
	
	
	int i;

	for(i=0; i< 5; i++){
      
      
       printf("Hola AQUIIII \n");
       
       Task *t;
       
       t = taskrunqueue.heap;
         
       if(t!=null)
		 printf("Found Task if \n");
		 //Print Info
		 // while(t.next!=null)
		 //  {  
		 //     t = t.next;
		 //     t.printf(getId());
		 //     if(t == t.tail){
		//		  t.printf(getId());
		//		  break;
		 //       }
		 //  }
	   else
	     printf("Found Task  else \n");
	
	   
      }
      taskyield();
      
  }



void taskmain(int argc, char **argv) {
  counter = 0;
  int i;
 
  taskcreate(taskadn,(void*)i,STACK);
  for (i=0; i < 50; i++) {
    taskcreate(myTask,(void*)i,STACK);
   
    
  }
  
}
