#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <string.h>

#define BUFFER_SIZE 10

// Struct for Data Item
typedef struct
{
    char key[6];
    int pid;
    char flag;
    time_t produced_time;
} DataItem;

// Struct for Shared memory
typedef struct {
    DataItem buffer[BUFFER_SIZE];
    unsigned int producer_index;
    unsigned int consumer_index;
} SharedMemory;

// All functions
DataItem produce_data_item(char key[6], int producer_pid, char flag);
SharedMemory* create_shared_memory();
void *generate_key(void *arg);
void producer(SharedMemory * shared_memory);
void consumer(SharedMemory * shared_memory);
void signal_handler();
void signal_handler2();
void *verifier();

// Shared memory reference
SharedMemory* shared_memory;
pid_t pid = 0;
int total_key = 0;


int main(){
    pid = getpid();
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler2);

    pthread_t generate_thread_id;
    pthread_create(&generate_thread_id, NULL, generate_key, NULL);
    pthread_join(generate_thread_id, NULL);
    
    return 0;
}

// All functions implementation
DataItem produce_data_item(char key[6], int producer_pid, char flag){
    DataItem item;
    strcpy(item.key, key);
    item.pid = producer_pid;
    item.flag = flag;
    item.produced_time = time(NULL);
    return item;
}

SharedMemory* create_shared_memory(){
    SharedMemory* shared_memory;
    key_t key = ftok(".", getpid());
    int shmid = shmget(key, sizeof(SharedMemory), IPC_CREAT | 0666);
    if(shmid==-1){
        perror("Error in shmget");
        return NULL;
    }
    shared_memory = (SharedMemory*)shmat(shmid, NULL, 0);
    if(shared_memory==(SharedMemory*)-1){
        perror("Error in shmat");
        return NULL;
    }
    DataItem item = produce_data_item("12345",1, 'd');
    for(int i = 0; i < BUFFER_SIZE; i++){
        shared_memory->buffer[i] = item;
    }
    shared_memory->producer_index = 0;
    shared_memory->consumer_index = 0;
    return shared_memory;
}

void *generate_key(void *arg){
    FILE *keys_file;
    int key_length = 5;
    int key_count;
    printf("How many KEY you want? ");
    scanf("%d", &key_count);
    total_key = key_count;
    keys_file = fopen("keys.txt", "w");

    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    while (key_count-- > 0){
        char *key = malloc((key_length + 1) * sizeof(char));
        if (key == NULL){
            return NULL;
        }
        for (int i = 0; i < key_length; i++){
            int random_index = rand() % (sizeof(charset) - 1);
            key[i] = charset[random_index];
        }
        key[key_length] = '\0';
        fprintf(keys_file, "%s\n", key);
        free(key);
    }
    fclose(keys_file);
    printf("KEY generated\n");
    kill(pid, SIGUSR1);
}

void producer(SharedMemory * shared_memory){
    FILE *keys_file;
    FILE *produced_file;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    keys_file = fopen("keys.txt", "r");
    produced_file = fopen("produced.txt", "w");
    if (keys_file == NULL)
        exit(EXIT_FAILURE);
    while ((read = getline(&line, &len, keys_file)) != -1) {
        if(shared_memory->producer_index >= BUFFER_SIZE){
            shared_memory->producer_index = 0;
        }

        while(shared_memory->buffer[shared_memory->producer_index].flag == 'p'){
            sleep(0.100);
            if(feof(keys_file)) return;
            continue;
        }
        if(shared_memory->buffer[shared_memory->producer_index].flag == 'c' || shared_memory->buffer[shared_memory->producer_index].flag == 'd'){
            DataItem item = produce_data_item(line, getpid(), 'p');
            shared_memory->buffer[shared_memory->producer_index] = item;
            char *key  = shared_memory->buffer[shared_memory->producer_index].key;
            key[5] = '\0';
            int process_id = shared_memory->buffer[shared_memory->producer_index].pid;
            time_t produced_time = shared_memory->buffer[shared_memory->producer_index].produced_time;
            fprintf(produced_file, "KEY:%s PID:%d Date:%s", key, process_id, ctime(&produced_time));
            shared_memory->producer_index++;
        }
    }
    fclose(keys_file);
    fclose(produced_file);
    if (line)
        free(line);
    printf("producer done \n");
    exit(0);
}

void consumer(SharedMemory * shared_memory){
    FILE *consumed_file;
    consumed_file = fopen("consumed.txt", "w");
    printf("inside consumer ");
    
    while(total_key > 0){
        if(shared_memory->consumer_index >= BUFFER_SIZE){
            shared_memory->consumer_index = 0;
        }
        if(shared_memory->buffer[shared_memory->consumer_index].flag == 'p'){
            char *key  = shared_memory->buffer[shared_memory->consumer_index].key;
            key[5] = '\0';
            int process_id = shared_memory->buffer[shared_memory->consumer_index].pid;
            time_t produced_time = shared_memory->buffer[shared_memory->consumer_index].produced_time;
            fprintf(consumed_file, "KEY:%s PID:%d Date:%s", key, process_id, ctime(&produced_time));
            shared_memory->buffer[shared_memory->consumer_index].flag = 'c';
            shared_memory->consumer_index++;
            total_key--;
        }
    }
    fclose(consumed_file);
    printf("consumer done \n");
    exit(0);
}
void signal_handler(){
    printf("inside signal handler\n");
    SharedMemory * shared_memory = create_shared_memory();
    pid_t producer_pid, consumer_pid;
    producer_pid = fork();
    if (producer_pid == 0){
        producer(shared_memory);
    }

    consumer_pid = fork();
    if (consumer_pid == 0){
        consumer(shared_memory);
    }
    wait(NULL);
    wait(NULL);
    kill(pid, SIGUSR2);
}

void signal_handler2(){
    pthread_t verifier_thread_id;
    pthread_create(&verifier_thread_id, NULL, verifier, NULL);
    pthread_join(verifier_thread_id, NULL);
}

void *verifier(){
    FILE *produced_file;
    FILE *consumed_file;
    char * produced_data = NULL;
    char * consumed_data = NULL;
    size_t len = 0;
    ssize_t read1, read2;
    produced_file = fopen("produced.txt", "r");
    consumed_file = fopen("consumed.txt", "r");
    int c = 0;
    if (produced_file == NULL || consumed_file == NULL)
        exit(EXIT_FAILURE);
    while((read1 = getline(&produced_data, &len, produced_file)) != -1 && (read2 = getline(&consumed_data, &len, consumed_file)) != -1){
        char p_key[5];
        char c_key[5];
        strncpy(p_key, produced_data+4, 5);
        strncpy(c_key, consumed_data+4, 5);
        int value = strcmp(p_key, c_key);
        if(strcmp(p_key, c_key) != 0){
            printf("Not varified \n");
            return NULL;
        }
    }
    fclose(produced_file);
    fclose(consumed_file);
    printf("Varified!");
}