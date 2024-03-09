
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// // HW3: Parse the new arguments too
// void getargs(int *port, int* threads_num, int* queue_max_size, char** schedalg, int argc, char *argv[])
// {
//     if (argc < 5 || argc >5) {
// 	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
// 	exit(1);
//     }
//     *port = atoi(argv[1]);              //should we add try&catch?
//     *threads_num = atoi(argv[2]);
//     *queue_max_size = atoi(argv[3]);
//     *schedalg = (argv[4]);
// }



// int main(int argc, char *argv[])
// {

//     int listenfd, connfd, port, clientlen;

//     int threads_num, queue_max_size;
//     char* schedalg;

// //int argc2 = 5;
// //char* argv2 = {"2", "2", "4", "fd"};
//     getargs(&port, &threads_num, &queue_max_size, &schedalg, argc, argv);
//     if (queue_max_size==0 || threads_num==0){       //check invalid schedalg
//         printf ("invalid input\n");          //check format
//     }

//     printf("%s\n", schedalg);
//     if (strcasecmp(schedalg, "block")==0){
//         printf("ok\n");
//     }
//     else{
//         printf("bad\n");
//     }
// }



    struct Queue{
        int* c_queue;
        int c_queue_max_size;
        int c_first;
        int c_last;
        int c_size;
    };
   
   struct Queue* create_queue (int n){
        struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
        q->c_queue_max_size = n;
        q->c_queue = (int*)malloc(sizeof(int)*q->c_queue_max_size);
        q->c_first = 0;
        q->c_last = 0;
        q->c_size = 0;
        return q;
    }

    void add (struct Queue* q, int val){           //think about using pointers
        if (q->c_size<q->c_queue_max_size){
            q->c_queue[q->c_last] = val;
            q->c_last = (q->c_last+1)%q->c_queue_max_size;
            q->c_size++;
        }
        return;
    }

    // void remove_by_index (struct Queue* q, int index){         //great potential of bugs
    //     if (q->c_size>index && index>0){
    //         for (int j = index; j!=q->c_first; j= (j-1+q->c_queue_max_size)%q->c_queue_max_size){
    //             q->c_queue[j] = q->c_queue[(j-1+q->c_queue_max_size)%q->c_queue_max_size];
    //         }
    //         q->c_first = (q->c_first+1)%q->c_queue_max_size;
    //         q->c_size--;
    //     }
    // }

    // int remove_by_index (struct Queue* q, int index){         //great potential of bugs
    //     if (q->c_size>index && index>=0){
    //         int i = q->c_first;
    //         struct task_info temp = q->c_queue[i];

    //         for (int j = 0; j<index; j++){
    //             struct task_info temp2 = q->c_queue[(i+1)%q->c_queue_max_size];
    //             q->c_queue[(i+1)%q->c_queue_max_size] = temp;
    //             temp = temp2;
    //             i = (i+1)%q->c_queue_max_size;
    //         }
    //         q->c_first = (q->c_first+1)%q->c_queue_max_size;
    //         q->c_size--;
    //         return temp.id;
    //     }
    //     else{
    //         printf ("Invalid index");                           //print error to stderr
    //         return -1;
    //     }
    // }


    /*
        void remove_value (struct Queue* q, int value){         //great potential of bugs
            if (q->c_size>0){
                for (int i = q->c_first; i!=q->c_last; i= (i+1)%q->c_queue_max_size){
                    if (q->c_queue[i]==value){
                        for (int j = i; j!=q->c_first; j= (j-1+q->c_queue_max_size)%q->c_queue_max_size){
                            q->c_queue[j] = q->c_queue[(j-1+q->c_queue_max_size)%q->c_queue_max_size];
                        }
                        q->c_first = (q->c_first+1)%q->c_queue_max_size;
                        q->c_size--;
                        break;
                    }
                }
            }
        }
    */

    void pop (struct Queue* q){
        if (q->c_size>0){
            q->c_first = (q->c_first+1)%q->c_queue_max_size;
            q->c_size--;
        }
    }

    int top (struct Queue* q){
        if (q->c_size>0){
            return q->c_queue[q->c_first];
        }
        else{
            printf ("Invalid, queue is empty");              //other error value?
            return -1;
        }
    }

    void delete_queue(struct Queue* q){
        free (q->c_queue);
        free (q);
    }


    int remove_by_index (struct Queue* q, int index){         //great potential of bugs
        if (q->c_size>index && index>=0){
            int return_value = top(q);
            int size = q->c_size;
            for (int j = 0; j<size; j++){
                int temp = top(q);
                pop(q);
                if (j==index){
                    return_value = temp; 
                }
                else{
                    add(q, temp);
                }
            }
            return return_value;
        }
        else{
            printf ("Invalid index");                           //print error to stderr
            return -1;
        }
    }


    void drop_random (struct Queue* waiting_tasks){
    int drop_count = (waiting_tasks->c_size+1)/2;
    for(int i = 0; i<drop_count; i++) {
        if(waiting_tasks->c_size == 0){
            printf ("Invalid, queue is empty");              //other error value?
            break;
        }    
        int random_index = rand();
        printf ("rand is: %d\n", random_index);
        printf ("current size is: %d\n", waiting_tasks->c_size);

        random_index = random_index % waiting_tasks->c_size;
        printf ("want to close: %d\n", random_index);
        int id_to_close = remove_by_index(waiting_tasks, random_index);
        printf ("closed: %d\n", id_to_close);
        //Close (id_to_close);
        //Close(waiting_tasks->c_queue[random_index].id);        //not good
        //cond_signal (&c2);          //think about it
    }
}


int main(int argc, char *argv[])
{

    struct Queue* q = create_queue (10);

    for (int i = 0; i<8; i++){
        add(q, i);
    }

    for (int i = 0; i<5; i++){
        pop(q);
    }

    for (int i = 0; i<5; i++){
        add(q, i);
    }

    for (int i = 0; i<q->c_size; i++){
        printf("%i is %d\n", i, q->c_queue[i]);
    }

    printf ("size is before: %d\n", q->c_size);
    //drop_random(q);
    remove_by_index (q, 4);
    printf ("size is after: %d\n", q->c_size);

    int size = q->c_size;
    for (int i = 0; i<size; i++){
        printf("%i is %d\n", i, top(q));
        pop(q);
    }


}
