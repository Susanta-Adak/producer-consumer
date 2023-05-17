#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 10

pid_t pid = 0;

void *generate_key(void *arg){
    FILE *file;
    int key_length = 5;
    int key_count;
    printf("How many KEY you want? ");
    scanf("%d", &key_count);
    file = fopen("input.txt", "a");

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
        fprintf(file, "%s\n ", key);
        free(key);
    }
    fclose(file);
    printf("KEY generated! %d\n", pid);
    kill(pid, SIGUSR1);
    // return NULL;
}


void producer(){
    for (int i = 0; i < 10; i++){
        printf("From producer %d \n", i);
    }
}

void consumer(){
    for (int i = 0; i < 10; i++){
        printf("From consumer %d \n", i);
    }
}
void signal_handler(){
    printf("inside signal handler\n");
    pid_t producer_pid, consumer_pid;
    producer_pid = fork();
    if (producer_pid == 0){
        producer();
        exit(0);
    }

    consumer_pid = fork();
    if (consumer_pid == 0){
        consumer();
        exit(0);
    }
    // wait(NULL);
    // wait(NULL);
}


int main(){
    pid = getpid();
    signal(SIGUSR1, signal_handler);

    pthread_t generate_thread_id;
    pthread_create(&generate_thread_id, NULL, generate_key, NULL);
    pthread_join(generate_thread_id, NULL);
    
    return 0;
}
