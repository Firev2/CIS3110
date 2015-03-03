/*-----------------
a2.c
By: Alex Schwarz
SID: 0719732
For: CIS*3110 A2
-------------------*/
#include <string.h>
#include "queue.h"
#include "Boolean.h"

#define _BOOLEAN

typedef enum {

    ARRIVAL,
    PREEMPTION,
    IO_BURST,
    IO_COMPLETE,
    CPU_BURST,
    THREAD_TERMINATION,
    
} event_type;

typedef enum {

    ARRIVED,
    READY,
    BLOCKED,
    RUNNING,
    TERMINATED

} state; /* thread states */

typedef struct { /* a thread holds N jobs */

    int orig_cpu_time;
    int cpu_time;
    int io_time;
    int num;

} job;

typedef struct { /* a process holds N threads */

    int t_id;
    int p_id;

    state current_state;

    int arrival_time;
    int service_time;

    int num_jobs;
    job** jobs;
    int current_job;

    int TaT;
    int IO_time;
    int finish_time;

} thread;

typedef struct {

    thread** threads;
    int num_threads;
    int current_thread;    

    int pid;

    int TaT;

} process;

typedef struct {

    thread* t;
    process* p;
    event_type type;
    int time;
    /* an event contains a thread and it's process, an event type, and an event time. */
} event;

/* global system timer */
int systime = 0;

int quantum = 0;

/* statistics */
int total_io_time = 0;
int total_service_time = 0;
int total_switch_time = 0;
float cpu_util = 0.0f;
int avg_TaT = 0;

/* decides whether or not to schedule immediately or later */
Boolean cpu_idle = true;

/* cmd line arguments */
Boolean is_verbose = false;
Boolean is_detailed_mode = false;
Boolean is_RR = false;

/* filled from input file */
int num_processes = 0;
int thread_switch = 0;
int process_switch = 0;

process** p_arr;

/*
get_stats: retrieves information from p_arr for use in statistic calculation
returns no value.
*/
void get_stats() {

    int i;
    int j;
    int k;

    for(i = 0; i < num_processes-1; i++) { /* outer loop accesses procs */

         for(j = 0; j < p_arr[i]->num_threads; j++) { /* inner loop accesses threads */

             int service = 0;
             int io = 0;

             for(k = 0; k < p_arr[i]->threads[j]->num_jobs; k++) {
                 service +=  p_arr[i]->threads[j]->jobs[k]->orig_cpu_time;
                 io += p_arr[i]->threads[j]->jobs[k]->io_time;
             }

             total_service_time += service;
             total_io_time += io;

             p_arr[i]->threads[j]->service_time = service;
             p_arr[i]->threads[j]->TaT = p_arr[i]->threads[j]->finish_time - p_arr[i]->threads[j]->arrival_time;
             p_arr[i]->threads[j]->IO_time = io;

         }
          
         avg_TaT += p_arr[i]->TaT;
    }    

    if(num_processes-1 ==  0) {
        avg_TaT = p_arr[0]->TaT;
      
    }
    else
        avg_TaT = avg_TaT / (num_processes-1);
}
/*
print_details: Prints statistical info about threads to standard output: used with -d
returns no value
*/
void print_details() {

    int i;
    int j;
    
    for(i = 0; i < num_processes-1; i++) {
        
        for(j = 0; j < p_arr[i]->num_threads; j++) {
            
            printf("Thread %d of Process %d:\n",p_arr[i]->threads[j]->t_id, p_arr[i]->pid);
            printf("arrival time: %d\n", p_arr[i]->threads[j]->arrival_time);
            printf("service time: %d units\n", p_arr[i]->threads[j]->service_time);
            printf("I/O time: %d units\n", p_arr[i]->threads[j]->IO_time);
            printf("turnaround time: %d units\n", p_arr[i]->threads[j]->TaT);
            printf("finish time: %d units\n", p_arr[i]->threads[j]->finish_time);
            printf("\n");
        }

    }
}

/*
print_statistics: Prints info about the scheduling system's inputs
returns no value.
*/
void print_statistics() {

    if(is_RR == false)
        printf("\nFCFS:\n\n");
    else {
        printf("Round Robin (quantum = %d time units):\n\n",quantum);
    }

    get_stats();

    float idle_time = ((total_io_time + total_switch_time)/(float)systime) * 100;
    cpu_util = 100.0 - idle_time;

    printf("Total time required is %d time units\n",systime);
    printf("Average Turnaround Time is %d time units\n",avg_TaT);
    printf("CPU Utilization is %f \n\n",cpu_util);

    if(is_detailed_mode == true) {
        /* print thread/process info */
        print_details();
    }

}

/*
read_input_file(FILE* inputfile, pri_queue q): parses a file inputfile and pushes an event to q
for each thread arrival - more or less starts the scheduling cycle. 

returns false if file input is wrong, file does not exist, or q is NULL. returns true on success.
*/
Boolean read_input_file(FILE* inputfile, pri_queue q) {

    int i;
    int j;
    int k;

    if(q == NULL)
        return false;

    if(inputfile == (FILE*)NULL) {
       printf("Unable to find input file.\n");
       return false;
    }

    /* first line in file: get number of procs, plus overheads */
    fscanf(inputfile, "%d %d %d\n", &num_processes, &thread_switch, &process_switch);

    p_arr = (process**)malloc(sizeof(process*)*num_processes);

    if(p_arr == NULL) {
        printf("Mem alloc error\n");
        return false;
    }

    for(i = 0; i < num_processes; i++) { /* outer loop scans processes */

        process* p = (process*)malloc(sizeof(process)*1);

        if(p == (process*)NULL) {
            printf("Mem alloc error.\n");
            return false;
        }

        fscanf(inputfile,"%d %d\n",&p->pid, &p->num_threads);
        /* process number, number of threads */
     
        p->threads = (thread**)malloc(sizeof(thread*) * p->num_threads);      

        for(j = 0; j < p->num_threads; j++) { /* inner loop scans threads */

            thread* t = (thread*)malloc(sizeof(thread)*1);
            t->p_id = i+1;
            t->current_job = 0;
            fscanf(inputfile,"%d %d %d\n", &t->t_id, &t->arrival_time, &t->num_jobs);
            /* thread number, arrival time, number of cpu */
            p->threads[j] = t;

            t->jobs = (job**)malloc(sizeof(job*)*t->num_jobs);

            for(k = 0; k < t->num_jobs; k++) { /* inner-inner loop scans jobs */

                job* j = (job*)malloc(sizeof(job)*1);

                if(k == t->num_jobs - 1) {

                    fscanf(inputfile,"%d %d\n", &j->num,&j->cpu_time);
                    j->orig_cpu_time = j->cpu_time;
                    j->io_time = 0;
                    /* job num, cpu_time */
                }
                else { /* job num, cpu_time, io_time */
                    fscanf(inputfile,"%d %d %d\n", &j->num,&j->cpu_time,&j->io_time);
                    j->orig_cpu_time = j->cpu_time;
                }

                t->jobs[k] = j;
            }

            event* e = (event*)malloc(sizeof(event)*1);
            e->t = t;
            e->p = p;
            e->time = t->arrival_time;
            e->type = ARRIVAL;
            priq_push(q,e,e->time);
        }

        p_arr[i] = p;

    }

    num_processes = i + 1;
    return true;
}

/*
process_cmd_line(argc,argv): reads the command line to decide whether detailed or verbose mode is requested,
along with round robin mode.
returns no value
*/
void process_cmd_line(int argc, char* argv[]) {

    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            is_detailed_mode = true;            
        }
        else if(strcmp(argv[i], "-v") == 0) {
            is_verbose = true;
        }
        else if (strcmp(argv[i], "-r") == 0) {
            is_RR = true;
            quantum = atoi(argv[i+1]);
            if(quantum <= 0) {
                printf("WARNING: RR quantum was 0 or negative.\n");
            }
        }
    }

}

/*
print_verbose(int time, event* e, char* state1, char* state2): prints a state transition message. Used in -v mode
returns no value
*/
void print_verbose(int time, event* e, char* state1, char* state2) {

    printf("At time %d: Thread %d of Process %d moves from %s to %s\n", time, e->t->t_id, e->p->pid, state1, state2);
}


/*
do_RR:


*/
void do_RR() {

    int priority;

    int current_pid = -1; /* start these as -1 so that first thread does a context switch */
    int current_tid = -1;

    int overhead = 0;

    pri_queue EventQ = priq_new(0); /* our priority queue */

    read_input_file(stdin,EventQ); /* read input file */

    while(!is_empty(EventQ)) {

        event* e = priq_pop(EventQ,&priority);
         
        systime = e->time;

        int cpytime = systime;

        if(e->t->p_id != current_pid && e->type != ARRIVAL) {

            cpytime += process_switch;
            current_pid = e->t->p_id;
            current_tid = -1;
            total_switch_time += process_switch;
            overhead += process_switch;
        } /* check if process is different. if so, thread is also different. */

        if(e->t->t_id != current_tid && e->type != ARRIVAL) {

            cpytime += thread_switch;
            current_tid = e->t->t_id;
            total_switch_time += thread_switch;
            overhead += thread_switch;
        } /* check if current events thread is different from last event's */


        switch(e->type) {

            case ARRIVAL: {

               e->t->current_job = 0;
               e->p->current_thread = 0;

               if(is_verbose)
                    print_verbose(cpytime,e,"new","ready");

               if(cpu_idle == true) {

                    e->t->current_state = RUNNING;
                    e->time = cpytime;
                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);

                }
                else {
event* e1 = priq_top(EventQ,&priority);
                    e->t->current_state = READY;

                    switch(e1->type) { /* throw to end of queue. switch to decide how long the last processes event is an
d add behind that. */

                        case ARRIVAL: {
                            e->time = e1->time + overhead;
                        }break;

                        case CPU_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->cpu_time + overhead;
                        }break;
            
                        case IO_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->io_time + overhead;
                        }break;
 
                        case PREEMPTION:
                        case IO_COMPLETE: {
                            e->time = e1->time + overhead;
                        }break;            

                        case THREAD_TERMINATION: {
                            e->time = e1->time + overhead;
                        }break;
                    };

                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);
                }

             }break;
 
             case CPU_BURST: {

                 cpu_idle = false;

                 if(is_verbose)
                      print_verbose(cpytime,e,"ready","running");

                 int quanttime = cpytime + quantum;

                 if(quanttime <  (cpytime + e->t->jobs[e->t->current_job]->cpu_time)) {
                      
                     e->time = quanttime;
                     e->type = PREEMPTION;
                     cpu_idle = true;
                     e->t->jobs[e->t->current_job]->cpu_time -= quantum;

                     if(e->t->jobs[e->t->current_job]->cpu_time < 0)
                         e->t->jobs[e->t->current_job]->cpu_time = 0;
                     priq_push(EventQ,e,e->time);
                     
                 }
                 else {
                     if(e->t->current_job == e->t->num_jobs-1) {
                         
                         e->t->current_state = RUNNING;
                         e->time = cpytime + e->t->jobs[e->t->current_job]->cpu_time;
                         e->type = THREAD_TERMINATION;
                         priq_push(EventQ,e,e->time);

                    }
                    else { /* ...otherwise let it continue and do I/O */
                        e->t->current_state = RUNNING;
                        e->time = cpytime + e->t->jobs[e->t->current_job]->cpu_time;
                        e->type = IO_BURST;
                        
                        priq_push(EventQ,e,e->time);
                    }

                 }
             }break;

             case IO_BURST: {

                 e->t->current_state = BLOCKED;

                 if(is_verbose)
                    print_verbose(cpytime,e,"running","blocked");

                 cpu_idle = true;
                 e->time = cpytime + e->t->jobs[e->t->current_job]->io_time;
                 e->type = IO_COMPLETE;
                 priq_push(EventQ,e,e->time); /* push I/O complete event after current jobs IO time */



             }break;

             case IO_COMPLETE: {
               
                if(is_verbose)
                        print_verbose(cpytime,e,"blocked","ready");

                e->t->current_job += 1;

                if(cpu_idle == true) { /* if cpu is idle, we can schedule immediately */

                    e->t->current_state = RUNNING;
                    e->time = cpytime;
                    e->type = CPU_BURST;
                    cpu_idle = false;

                    priq_push(EventQ,e,e->time);

                }
                else {
                    event* e1 = priq_top(EventQ,&priority);
                    e->t->current_state = READY;

                    switch(e1->type) { /* throw to end of queue. switch to decide how long the last processes event is an
d add behind that. */

                        case ARRIVAL: {
                            e->time = e1->time + overhead;
                        }break;

                        case CPU_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->cpu_time + overhead;
                        }break;
                 
                        case IO_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->io_time + overhead;
                        }break;
   
                        case IO_COMPLETE:
                        case PREEMPTION: {
                            e->time = e1->time + overhead;
                        }break;                 

                        case THREAD_TERMINATION: {
                            e->time = e1->time + overhead;
                        }break;
                    };

                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);


                }
             }break;

             case THREAD_TERMINATION: {

                if(is_verbose)
                    print_verbose(cpytime,e,"running","terminated");

                cpu_idle = true;

                e->t->current_state = TERMINATED;
                e->t->finish_time = cpytime;
                e->p->current_thread += 1;

                if(e->p->current_thread == e->p->num_threads) { /* process is now finished. grab its TaT for stats later */

                    e->p->TaT = cpytime;

                }

             }break;

             case PREEMPTION: {

                 if(is_verbose)
                     print_verbose(cpytime,e,"running","ready");
               
                 if(cpu_idle == true) {

                    e->time = cpytime;
                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);
                 }

             }break;

        };
        overhead = 0;
    }

}

/*
do_fcfs(): next-event simulation for cpu scheduling using the first-come-first-serve algorithm
returns no value
*/
void do_fcfs() {

    int priority; 

    int current_pid = -1; /* start these as -1 so that first thread does a context switch */
    int current_tid = -1;

    int overhead = 0; /* thread/context switch overhead */

    pri_queue EventQ = priq_new(0); /* our priority queue */

    read_input_file(stdin,EventQ);

    while(!is_empty(EventQ)) {

        event* e = priq_pop(EventQ,&priority);

        systime = e->time;

        int cpytime = systime;

        if(e->t->p_id != current_pid && e->type != ARRIVAL) {
            
            cpytime += process_switch;
            current_pid = e->t->p_id;
            current_tid = -1;
            total_switch_time += process_switch;
            overhead += process_switch;
        } /* check if process is different. if so, thread is also different. */
        
        if(e->t->t_id != current_tid && e->type != ARRIVAL) {

            cpytime += thread_switch;
            current_tid = e->t->t_id;
            total_switch_time += thread_switch;
            overhead += thread_switch;
        } /* check if current events thread is different from last event's */

        switch(e->type) {

            case ARRIVAL: {

               e->t->current_job = 0;
               e->p->current_thread = 0;

               if(is_verbose)
                    print_verbose(cpytime,e,"new","ready");

                if(cpu_idle == true) {

                    e->t->current_state = RUNNING;
                    e->time = cpytime;
                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);
                
                }
                else { /* put to end of queue */
                    
                    event* e1 = priq_top(EventQ,&priority);       
                    e->t->current_state = READY;
                   
                    switch(e1->type) { /* throw to end of queue. switch to decide how long the last processes event is an
d add behind that. */

                        case ARRIVAL: {
                            e->time = e1->time + overhead;
                        }break;

                        case CPU_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->cpu_time + overhead;
                        }break;
                
                        case IO_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->io_time + overhead;
                        }break;      
                
                        case PREEMPTION:
                        case IO_COMPLETE: {
                            e->time = e1->time + overhead;
                        }break;


                        case THREAD_TERMINATION: {
                            e->time = e1->time + overhead;
                        }break;
                    };

                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);
                }
      
            }break;

            case CPU_BURST: {

                cpu_idle = false;

                if(is_verbose)
                        print_verbose(cpytime,e,"ready","running");

                /* if thread is at its last job, set up termination */
                if(e->t->current_job == e->t->num_jobs-1) {
                     e->t->current_state = RUNNING;
                     e->time = cpytime + e->t->jobs[e->t->current_job]->cpu_time;
                     e->type = THREAD_TERMINATION;
                     cpu_idle = false;
                     priq_push(EventQ,e,e->time);

                }
                else { /* ...otherwise let it continue and do I/O */
                    e->t->current_state = RUNNING;
                    e->time = cpytime + e->t->jobs[e->t->current_job]->cpu_time;
                    e->type = IO_BURST;
                    cpu_idle = false;
                    priq_push(EventQ,e,e->time);
                 
                }
   
            }break;

            case IO_BURST: {

                e->t->current_state = BLOCKED;

                    if(is_verbose)
                        print_verbose(cpytime,e,"running","blocked");

                    cpu_idle = true;
                    e->time = cpytime + e->t->jobs[e->t->current_job]->io_time;
                    e->type = IO_COMPLETE;
                    priq_push(EventQ,e,e->time); /* push I/O complete event after current jobs IO time */
                 
            }break;

            case IO_COMPLETE: {

                if(is_verbose)
                        print_verbose(cpytime,e,"blocked","ready");

                e->t->current_job += 1;

                if(cpu_idle == true) { /* if cpu is idle, we can schedule immediately */

                    e->t->current_state = RUNNING;
                    e->time = cpytime;
                    e->type = CPU_BURST;
                    cpu_idle = false;
                    
                    priq_push(EventQ,e,e->time);

                }
                else { /*  put to end of queue */

                    event* e1 = priq_top(EventQ,&priority);
                    e->t->current_state = READY;

                    switch(e1->type) { /* throw to end of queue. switch to decide how long the last processes event is and add behind that. */

                        case ARRIVAL: {
                            e->time = e1->time + overhead;
                        }break;
                        
                        case CPU_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->cpu_time + overhead;
                        }break;
                        
                        case IO_BURST: {
                            e->time = e1->time + e1->t->jobs[e1->t->current_job]->io_time + overhead;
                        }break;

                        case PREEMPTION:
                        case IO_COMPLETE: {
                            e->time = e1->time + overhead;
                        }break;

                        case THREAD_TERMINATION: {
                            e->time = e1->time + overhead;
                        }break;
                    };

                    e->type = CPU_BURST;
                    priq_push(EventQ,e,e->time);
                }
 
             
            }break;

            case THREAD_TERMINATION: {

                if(is_verbose)
                    print_verbose(cpytime,e,"running","terminated");

                cpu_idle = true; 

                e->t->current_state = TERMINATED;
                e->t->finish_time = cpytime;
                e->p->current_thread += 1;
                
                if(e->p->current_thread == e->p->num_threads) { /* process is now finished. grab its TaT for stats later */

                    e->p->TaT = cpytime;
                    
                }

            }break;

            case PREEMPTION: {
            }break; /* not used for FCFS */

        };
        overhead = 0; /* reset overhead time each event */
    }
}


int main(int argc, char* argv[]) {

    process_cmd_line(argc,argv);

    if(is_RR == false)
        do_fcfs();
    else
        do_RR();

    print_statistics();

    return 0;
}
