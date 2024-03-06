#include "segel.h"
#include "request.h"

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//



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

    void add (struct Queue* q, int val){
        if (q->c_size<q->c_queue_max_size){
            q->c_queue[q->c_last] = val;
            q->c_last = (q->c_last+1)%q->c_queue_max_size;
            q->c_size++;
        }
        return;
    }

    void remove_by_index (struct Queue* q, int index){         //great potential of bugs
        if (q->c_size>index && index>0){
            for (int j = index; j!=q->c_first; j= (j-1+q->c_queue_max_size)%q->c_queue_max_size){
                q->c_queue[j] = q->c_queue[(j-1+q->c_queue_max_size)%q->c_queue_max_size];
            }
            q->c_first = (q->c_first+1)%q->c_queue_max_size;
            q->c_size--;
        }
    }

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
            return -1;              //other error value?
        }
    }

    void delete_queue(struct Queue* q){
        free (q->c_queue);
        free (q);
    }

    void drop_random (struct Queue* waiting_tasks){
    int drop_count = (waiting_tasks->c_size+1)/2;
    for(int i = 0; i<drop_count; i++) {
        if(waiting_tasks->c_size == 0){
            break;
        }    
        int random_index = rand() % waiting_tasks->c_size;
        Close(waiting_tasks->c_queue[random_index]);        //not good
        remove_by_index(waiting_tasks, random_index);
        cond_signal (&c2);          //think about it
    }
}


struct thread_args{
    struct Queue* waiting_tasks;
    struct Queue* running_tasks;
    char* schedalg;
};



pthread_cond_t c1, c2, c3; // should be initialized
pthread_mutex_t m; // should be initialized



// HW3: Parse the new arguments too
void getargs(int *port, int* threads_num, int* queue_max_size, char** schedalg, int argc, char *argv[])
{
    if (argc < 5 || argc >5) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);              //should we add try&catch?
    *threads_num = atoi(argv[2]);
    *queue_max_size = atoi(argv[3]);
    *schedalg = atoi(argv[4]);
}


int main(int argc, char *argv[])
{
    pthread_cond_init(&c1, NULL);
    pthread_cond_init(&c2, NULL);
    pthread_cond_init(&c3, NULL);
    pthread_mutex_init(&m, NULL);

    int listenfd, connfd, port, clientlen;

    int threads_num, queue_max_size;
    char* schedalg;

    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_max_size, &schedalg, argc, argv);
    if (queue_max_size==0 || threads_num==0){       //check invalid schedalg
        printf ("invalid input\n");          //check format
    }


    struct Queue* waiting_tasks = create_queue (queue_max_size);
    struct Queue* running_tasks = create_queue (threads_num);

    struct thread_args* args = (struct thread_args*)malloc(sizeof(struct thread_args));
    args->waiting_tasks = waiting_tasks;
    args->running_tasks = running_tasks;
    args->schedalg = schedalg;


    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t)*threads_num);

    for (int i = 0; i<threads_num; i++){
        Pthread_Create(&(threads[i]), NULL, thread_routine, args);
    }

    //make queue, create threads, listen (according to tutorial), accept request, insert queue



    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        mutex_lock(&m);
        if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
            if (strcasecmp(schedalg, "block")){
                while (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
                    cond_wait (&c2, &m);        //add wrap function?
                }
            }
            else if (strcasecmp(schedalg, "dt")){
                Close(connfd);
                continue;
            }
            else if (strcasecmp(schedalg, "dh")){
                int fd_for_remove = top (waiting_tasks);
                Close(fd_for_remove);
                pop (waiting_tasks);
            }
            else if (strcasecmp(schedalg, "bf")){
                while (waiting_tasks->c_size+running_tasks->c_size > 0){
                    cond_wait (&c3, &m);        //add wrap function?
                }
            }
            else if (strcasecmp(schedalg, "random")){
                drop_random(waiting_tasks);
                if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){     //check what to do in this case
                    Close(connfd);
                    continue;   
                }
            }
            else{
                if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){     //check what to do in this case
                    Close(connfd);
                    continue;
                }
            }
        }

        add (waiting_tasks, connfd);
        cond_signal (&c1);          //add wrap function?
        mutex_unlock (&m);          //add wrap function?
    }

//we will arrive here ??

    for (int i = 0; i<threads_num; i++){
        Pthread_Join(threads[i], NULL);
    }

    delete_queue(waiting_tasks);
    delete_queue(running_tasks);
    free (threads);
    free (args);
}




void thread_routine(struct thread_args* args){
    while (1){
        mutex_lock (&m);                  //add wrap function?
        while (args->waiting_tasks->c_size==0){
            cond_wait (&c1, &m);          //add wrap function?
        }
        int curr_task = top(args->waiting_tasks);
        pop (args->waiting_tasks);
        add (args->running_tasks, curr_task);
        mutex_unlock (&m);             //add wrap function?

        requestHandle(curr_task);
	    Close(curr_task);

        mutex_lock (&m);                    //add wrap function?
        pop (args->running_tasks);                  //should use remove_value(args->running_tasks, curr_task)
        if (strcasecmp(args->schedalg, "block")){
            cond_signal (&c2);                    //add wrap function?
        }
        else if (strcasecmp(args->schedalg, "bf") && (args->waiting_tasks->c_size+args->running_tasks->c_size == 0)){
            cond_signal (&c3);
        }
        mutex_unlock (&m);                 //add wrap function?
    }
}

