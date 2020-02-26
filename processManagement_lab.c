#include "processManagement_lab.h"

/**
 * The task function to simulate "work" for each worker process
 * TODO#3: Modify the function to be multiprocess-safe 
 * */
void task(long duration) {
    // simulate computation for x number of seconds
    usleep(duration * TIME_MULTIPLIER);
    sem_wait(sem_global_data);

    // TODO: protect the access of shared variable below
    // update global variables to simulate statistics
    ShmPTR_global_data->sum_work+=duration;
    
    ShmPTR_global_data->total_tasks++;
    if (duration%2 == 1) {
        ShmPTR_global_data->odd++;
    }
    if (duration < ShmPTR_global_data->min) {
        ShmPTR_global_data->min = duration;
    }
    if (duration > ShmPTR_global_data->max) {
        ShmPTR_global_data->max = duration;
    }
    // printf("%d\n", duration);
    // TODO wtf take a look later

    // release the semaphore because we are done modifying it
    sem_post(sem_global_data);

}


/**
 * The function that is executed by each worker process to execute any available job given by the main process
 * */
void job_dispatch(int i) {
    while (true) {
        sem_wait(sem_jobs_buffer[i]);
        // printf("dispatching semaphore %i \n", i);
       
        if (shmPTR_jobs_buffer[i].task_status != -1) {
            
            switch (shmPTR_jobs_buffer[i].task_type) {
            case 't':
                task(shmPTR_jobs_buffer[i].task_duration);
                // printf("worker %i got %c\n", i, shmPTR_jobs_buffer[i].task_type);
                break;
            case 'w':

                usleep(shmPTR_jobs_buffer[i].task_duration * TIME_MULTIPLIER);
                /// printf("worker %i got %c waiting doing task\n", i, shmPTR_jobs_buffer[i].task_type);
                break;
            case 'z':
                exit(3);
                // printf("worker %i got %c exiting\n", i, shmPTR_jobs_buffer[i].task_type);
                break;
            case 'i':
                kill(getpid(), SIGKILL);
                // printf("worker %i got %c premature kill\n", i, shmPTR_jobs_buffer[i].task_type);
                break;
            }
            shmPTR_jobs_buffer[i].task_status = 0;
        }
    }
}

/** 
 * Setup function to create shared mems and semaphores
 * **/
void setup() {

    ShmID_global_data = shmget(IPC_PRIVATE, sizeof(global_data), IPC_CREAT | 0666);
    if (ShmID_global_data == -1) {
        printf("Global data shared memory creation failed\n");
        exit(EXIT_FAILURE);
    }
    ShmPTR_global_data = (global_data *) shmat(ShmID_global_data, NULL, 0);
    if ((int) ShmPTR_global_data == -1) {
        printf("Attachment of global data shared memory failed \n");
        exit(EXIT_FAILURE);
    }

    //set global data min and max
    ShmPTR_global_data->max = -1;
    ShmPTR_global_data->min = INT_MAX;
    // return

    // create the sem with value 1
    sem_global_data = sem_open("semglobaldata", O_CREAT | O_EXCL, 0644, 1);

    while (true) {
        if (sem_global_data == SEM_FAILED) {
            sem_unlink("semglobaldata");
            sem_global_data = sem_open("semglobaldata", O_CREAT | O_EXCL, 0644, 1);
        } else {
            break;
        }
    }

    // shared memmory
    ShmID_jobs = shmget(IPC_PRIVATE, sizeof(job) * number_of_processes, IPC_CREAT | 0666);
    if (ShmID_jobs == -1) {
        printf("creation failed line 100\n");
        exit(EXIT_FAILURE);
    }
    shmPTR_jobs_buffer = (job *) shmat(ShmID_jobs, NULL, 0);
    if ((int) shmPTR_jobs_buffer == -1) {
        printf("attachment fail line 105\n");
        exit(EXIT_FAILURE);
    }
    // TODO something still wrong here
    // clear buffer?

    for (int i = 0; i < number_of_processes; i++) {
        shmPTR_jobs_buffer[i].task_status = 0;
    }


    // part f
    for (int i = 0; i < number_of_processes; i++) {
        char *sem_name = malloc(sizeof(char) * 16);
        sprintf(sem_name, "semjobs%d", i);

        sem_jobs_buffer[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 0);
        while (true) {
            // if name clash, unlink then retry
            if (sem_jobs_buffer[i] == SEM_FAILED) {
                sem_unlink(sem_name);
                sem_jobs_buffer[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 0);
            } else break;
        }
    }

    return;
}

 
void createchildren() {

    // loop fork for number_of_processes
    for(int i = 0; i < number_of_processes; i++) {
        children_processes[i] = fork();
        if (children_processes[i] < 0) {
            fprintf(stderr, "forking failed");
            return;
        }

        if (children_processes[i] == 0) {
            // give job
            job_dispatch(i);
            break;
        }
    }
    return;
}

/**
 * The function where the main process loops and busy wait to dispatch job in available slots
 * */
void main_loop(char* fileName) {

    // load jobs and add them to the shared memory
    FILE* opened_file = fopen(fileName, "r");
    char action; //stores whether its a 'p' or 'w'
    long num; //stores the argument of the job 
    bool task_found; // possible task to assign

    while (fscanf(opened_file, "%c%ld\n", &action, &num) == 2) { //while the file still has input
        task_found = false;

        // continuously look for a possible process
        while (!task_found) {
            for (int i = 0; i < number_of_processes; i++) {
                int task_status = shmPTR_jobs_buffer[i].task_status;
                int child_status = waitpid(children_processes[i], NULL, WNOHANG); 
                

                if (task_status == 0 && child_status == 0) {
                    // give new job
                    task_found = true;
                    shmPTR_jobs_buffer[i].task_status = 1;
                    shmPTR_jobs_buffer[i].task_duration = num;
                    shmPTR_jobs_buffer[i].task_type = action;
                    sem_post(sem_jobs_buffer[i]);
                    // printf("%i assigned new job %c%i\n", i, shmPTR_jobs_buffer[i].task_type, shmPTR_jobs_buffer[i].task_duration)
                    break;


                } else if (child_status != 0) {
                    // dead child, fork it
                    children_processes[i] = fork();

                    if (children_processes[i] == 0) {
                        job_dispatch(i);
                        break;

                    } else {
                        task_found = true;
                        shmPTR_jobs_buffer[i].task_status = 1;
                        shmPTR_jobs_buffer[i].task_duration = num;
                        shmPTR_jobs_buffer[i].task_type = action;
                        sem_post(sem_jobs_buffer[i]);
                        break;
                    }
                }

            }
        }
    }

    fclose(opened_file);

    // printf("Main process is going to send termination signals\n");

    // terminate all workers
    // give all of them a z task if they're alive
    for (int i = 0; i < number_of_processes; i++)
    {
        int child_alive = waitpid(children_processes[i], NULL, WNOHANG);
        if (child_alive == 0)
        {
            shmPTR_jobs_buffer[i].task_type = 'z';
            shmPTR_jobs_buffer[i].task_duration = 0;
            shmPTR_jobs_buffer[i].task_status = 1;
            sem_post(sem_jobs_buffer[i]);
        }
    }

    //wait for all children processes to properly execute the 'z' termination jobs
    int process_waited_final = 0;
    pid_t wpid;
    while ((wpid = wait(NULL)) > 0)
    {
        process_waited_final++;
    }
    // printf("termination all done")

    // print final results
    printf("Final results: sum -- %ld, odd -- %ld, min -- %ld, max -- %ld, total task -- %ld\n", ShmPTR_global_data->sum_work, ShmPTR_global_data->odd, ShmPTR_global_data->min, ShmPTR_global_data->max, ShmPTR_global_data->total_tasks);
}

void cleanup(){

    int is_detached = shmdt((void *) ShmPTR_global_data); 
    int is_removed = shmctl(ShmID_global_data, IPC_RMID, NULL); 
    if (is_detached == -1 || is_removed == -1) {
        printf("global mem remove fail\n");
    }

    is_detached = shmdt((void *) shmPTR_jobs_buffer); 
    is_removed = shmctl(ShmID_jobs, IPC_RMID, NULL); 
        if (is_detached == -1 || is_removed == -1) {
        printf("job mem remove fail\n");
    }


    //unlink all semaphores before exiting process
    int sem_is_closed = sem_unlink("semglobaldata");

    for (int i = 0; i < number_of_processes; i++){
        char *sem_name = malloc(sizeof(char) * 16);
        sprintf(sem_name, "semjobs%d", i);
        sem_is_closed = sem_unlink(sem_name);
        free(sem_name);
    }
}

// Real main
int main(int argc, char* argv[]) {

    // printf("Lab 1 Starts...\n");

    struct timeval start, end;
    long secs_used,micros_used;

    //start timer
    gettimeofday(&start, NULL);

    //Check and parse command line options to be in the right format
    if (argc < 2) {
        printf("Usage: sum <infile> <numprocs>\n");
        exit(EXIT_FAILURE);
    }


    //Limit number_of_processes into 10. 
    //If there's no third argument, set the default number_of_processes into 1.  
    if (argc < 3) {
        number_of_processes = 1;
    }
    else{
        if (atoi(argv[2]) < MAX_PROCESS) number_of_processes = atoi(argv[2]);
        else number_of_processes = MAX_PROCESS;
    }

    setup();
    createchildren();
    main_loop(argv[1]);

    //parent cleanup
    cleanup();

    //stop timer
    gettimeofday(&end, NULL);

    double start_usec = (double) start.tv_sec * 1000000 + (double) start.tv_usec;
    double end_usec =  (double) end.tv_sec * 1000000 + (double) end.tv_usec;

    printf("Your computation has used: %lf secs \n", (end_usec - start_usec)/(double)1000000);


    return (EXIT_SUCCESS);
}