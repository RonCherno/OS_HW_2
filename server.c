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



    struct task_info{
        int id;
        struct timeval arrived;
    };




    struct Queue{
        struct task_info* c_queue;
        int c_queue_max_size;
        int c_first;
        int c_last;
        int c_size;
    };
   
   struct Queue* create_queue (int n){
        struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
        if (q==NULL){
            unix_error("malloc failed");
        }
        q->c_queue_max_size = n;
        q->c_queue = (struct task_info*)malloc(sizeof(struct task_info)*q->c_queue_max_size);
        if (q->c_queue==NULL){
            free (q);
            unix_error("malloc failed");
        }
        q->c_first = 0;
        q->c_last = 0;
        q->c_size = 0;
        return q;
    }

    void add (struct Queue* q, struct task_info val){
        if (q->c_size<q->c_queue_max_size){
            q->c_queue[q->c_last] = val;
            q->c_last = (q->c_last+1)%q->c_queue_max_size;
            q->c_size++;
        }
        return;
    }


    void pop (struct Queue* q){
        if (q->c_size>0){
            q->c_first = (q->c_first+1)%q->c_queue_max_size;
            q->c_size--;
        }
    }

    struct task_info top (struct Queue* q){
        if (q->c_size>0){
            return q->c_queue[q->c_first];
        }
        else{
            fprintf(stderr, "Invalid, queue is empty\n");
            struct timeval t;
            GetTimeOfDay(&t, NULL);
            struct task_info error_return = {-1, t};
            return (error_return);
        }
    }

    void delete_queue(struct Queue* q){
        free (q->c_queue);
        free (q);
    }


    int remove_by_index (struct Queue* q, int index){
        if (q->c_size>index && index>=0){
            struct task_info return_value = top(q);
            int size = q->c_size;
            for (int j = 0; j<size; j++){
                struct task_info temp = top(q);
                pop(q);
                if (j==index){
                    return_value = temp; 
                }
                else{
                    add(q, temp);
                }
            }
            return return_value.id;
        }
        else{
            fprintf(stderr, "Invalid index\n");
            return -1;
        }
    }


    void drop_random (struct Queue* waiting_tasks){
    int drop_count = (waiting_tasks->c_size+1)/2;
    for(int i = 0; i<drop_count; i++) {
        if(waiting_tasks->c_size == 0){
            fprintf(stderr, "Invalid, queue is empty\n");
            break;
        }    
        int random_index = rand() % waiting_tasks->c_size;
        int id_to_close = remove_by_index(waiting_tasks, random_index);
        Close (id_to_close);
    }
}


struct thread_args{
    struct Queue* waiting_tasks;
    struct Queue* running_tasks;
    char* schedalg;
    int id;
};



pthread_cond_t c1, c2, c3;
pthread_mutex_t m;




void getargs(int *port, int* threads_num, int* queue_max_size, char** schedalg, int argc, char *argv[])
{
    if (argc < 5 || argc >5) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *threads_num = atoi(argv[2]);
    *queue_max_size = atoi(argv[3]);
    *schedalg = (argv[4]);
}



void* thread_routine(void* arg){
        struct thread_args* args = (struct thread_args*) arg;
        int stat_req = 0;
        int dynm_req = 0;
        int total_req = 0;
        struct Threads_stats stats = {args->id, stat_req, dynm_req, total_req};


    while (1){
        Pthread_mutex_lock (&m);
        while (args->waiting_tasks->c_size==0){
            Pthread_cond_wait (&c1, &m);
        }
        struct task_info curr_task = top(args->waiting_tasks);
        int curr_task_id = curr_task.id;


        


        pop (args->waiting_tasks);
        add (args->running_tasks, curr_task);
        Pthread_mutex_unlock (&m);

        struct timeval dispatch;
        struct timeval curr_time;
        
        GetTimeOfDay(&curr_time, NULL);

        timersub(&curr_time, &curr_task.arrived, &dispatch);

        requestHandle(curr_task_id, curr_task.arrived, dispatch, &stats);
	    Close(curr_task_id);


        Pthread_mutex_lock (&m);
        pop (args->running_tasks);
        if (strcasecmp(args->schedalg, "block")==0){
            Pthread_cond_signal (&c2);
        }
        else if (strcasecmp(args->schedalg, "bf")==0 && (args->waiting_tasks->c_size+args->running_tasks->c_size == 0)){
            Pthread_cond_signal (&c3);
        }
        Pthread_mutex_unlock (&m);
    }
    //free (arg);
    return NULL;
}




int main(int argc, char *argv[])
{
    Pthread_cond_init(&c1, NULL);
    Pthread_cond_init(&c2, NULL);
    Pthread_cond_init(&c3, NULL);
    Pthread_mutex_init(&m, NULL);

    int listenfd, connfd, port, clientlen;

    int threads_num, queue_max_size;
    char* schedalg;

    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_max_size, &schedalg, argc, argv);
    if (queue_max_size==0 || threads_num==0){
        fprintf(stderr, "invalid input\n");
        return -1;
    }


    struct Queue* waiting_tasks = create_queue (queue_max_size);
    struct Queue* running_tasks = create_queue (threads_num);

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t)*threads_num);
    if (threads==NULL){
        unix_error("malloc failed");
    }

    for (int i = 0; i<threads_num; i++){
        struct thread_args* args = (struct thread_args*)malloc(sizeof(struct thread_args));
        if (args==NULL){
            unix_error("malloc failed");
        }
        args->waiting_tasks = waiting_tasks;
        args->running_tasks = running_tasks;
        args->schedalg = schedalg;
        args->id = i;

        Pthread_Create(&(threads[i]), NULL, thread_routine, args);
    }




    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        struct timeval arrived;
        GetTimeOfDay(&arrived, NULL);
        struct task_info to_add = {connfd, arrived};

        Pthread_mutex_lock(&m);
        if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
            if (strcasecmp(schedalg, "block")==0){
                while (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
                    Pthread_cond_wait (&c2, &m);
                }
            }
            else if (strcasecmp(schedalg, "dt")==0){
                Close(connfd);
                Pthread_mutex_unlock (&m);
                continue;
            }
            else if (strcasecmp(schedalg, "dh")==0){
                if (waiting_tasks->c_size!=0){
                    int fd_for_remove = (top(waiting_tasks)).id;
                    Close(fd_for_remove);
                    pop (waiting_tasks); 
                }
                else{
                    Close(connfd);
                    Pthread_mutex_unlock (&m);
                    continue;
                }
            }
            else if (strcasecmp(schedalg, "bf")==0){
                while (waiting_tasks->c_size+running_tasks->c_size > 0){
                    Pthread_cond_wait (&c3, &m);
                }
                Close(connfd);
                Pthread_mutex_unlock (&m);
                continue;
            }
            else if (strcasecmp(schedalg, "random")==0){
                if (waiting_tasks->c_size!=0){
                    drop_random(waiting_tasks);
                    if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
                        Close(connfd);
                        Pthread_mutex_unlock (&m);
                        continue;   
                    }
                    add (waiting_tasks, to_add);
                    Pthread_cond_broadcast (&c1);
                    Pthread_mutex_unlock (&m);
                    continue;
                }
                else{
                    Close(connfd);
                    Pthread_mutex_unlock (&m);
                    continue;
                }
            }
            else{
                if (waiting_tasks->c_size+running_tasks->c_size >= queue_max_size){
                    Close(connfd);
                    Pthread_mutex_unlock (&m);
                    continue;
                }
            }
        }
        add (waiting_tasks, to_add);
        Pthread_cond_signal (&c1);
        Pthread_mutex_unlock (&m);
    }



    for (int i = 0; i<threads_num; i++){
        Pthread_Join(threads[i], NULL);
    }

    Pthread_cond_destroy(&c1);
    Pthread_cond_destroy(&c2);
    Pthread_cond_destroy(&c3);
    Pthread_mutex_destroy(&m);
    delete_queue(waiting_tasks);
    delete_queue(running_tasks);
    free (threads);
}



