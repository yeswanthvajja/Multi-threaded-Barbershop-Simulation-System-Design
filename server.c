#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define NUM_BARBERS 3
#define NUM_CHAIRS 3
#define SOFA_CAPACITY 3
#define STANDING_CAPACITY 5
#define PORT 8080
#define MAX_CUSTOMER_REQUESTS 100

typedef struct {
    int id;
    int isBusy;
    int customers_served;
} Barber;

typedef struct {
    int id;
    int socket;
    time_t arrival_time;
    int hasBeenSeated;
} Customer;

Barber barbers[NUM_BARBERS];
int free_chairs = NUM_CHAIRS;
int sofa_customers = 0;
int standing_customers = 0;
int total_customers_served = 0;
int total_waiting_time = 0;

typedef struct Spinlock Spinlock;

extern Spinlock* create_spinlock();
extern void destroy_spinlock(Spinlock* lock);
extern void spinlock_lock(Spinlock* lock);
extern void spinlock_unlock(Spinlock* lock);

Spinlock* chairs_mutex;
Spinlock* sofa_mutex;
Spinlock* register_mutex;
Spinlock* stats_mutex;
pthread_cond_t barber_available[NUM_BARBERS];
pthread_cond_t chair_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t sofa_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t standing_available = PTHREAD_COND_INITIALIZER;

void* barber_thread(void* arg);
void* customer_thread(void* arg);
int random_range(int min, int max);
int find_available_barber();
void log_activity_to_file(const char* activity);
void display_barbershop_status();
float calculate_average_wait_time();
void send_message_to_client(int client_sock, const char *message);
void process_customer_payment(Barber *barber);
void cut_customer_hair(Barber *barber);

int main() {
    srand(time(NULL));
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    chairs_mutex = create_spinlock();
    sofa_mutex = create_spinlock();
    register_mutex = create_spinlock();
    stats_mutex = create_spinlock();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CUSTOMER_REQUESTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_BARBERS; i++) {
        barbers[i].id = i;
        barbers[i].isBusy = 0;
        barbers[i].customers_served = 0;
        pthread_cond_init(&barber_available[i], NULL);
        pthread_t tid;
        pthread_create(&tid, NULL, barber_thread, (void*)&barbers[i]);
    }

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        Customer *customer = (Customer *)malloc(sizeof(Customer));
        customer->id = new_socket;
        customer->socket = new_socket;
        customer->hasBeenSeated = 0;
        pthread_t tid;
        pthread_create(&tid, NULL, customer_thread, (void*)customer);
    }

    close(server_fd);
    destroy_spinlock(chairs_mutex);
    destroy_spinlock(sofa_mutex);
    destroy_spinlock(register_mutex);
    destroy_spinlock(stats_mutex);
    return 0;
}

void* barber_thread(void* arg) {
    Barber *barber = (Barber *)arg;
    while (1) {
        spinlock_lock(chairs_mutex);
        while (!barber->isBusy) {
            spinlock_unlock(chairs_mutex);
            usleep(100000);
            spinlock_lock(chairs_mutex);
        }

        char log_msg[100];
        sprintf(log_msg, "Barber %d starts cutting hair", barber->id);
        log_activity_to_file(log_msg);
        printf("%s\n", log_msg);
        
        spinlock_unlock(chairs_mutex);
        sleep(random_range(7, 19));

        spinlock_lock(register_mutex);
        sprintf(log_msg, "Barber %d finished cutting hair and is accepting payment", barber->id);
        log_activity_to_file(log_msg);
        printf("%s\n", log_msg);

        sleep(random_range(1, 3));


        spinlock_lock(stats_mutex);
        barber->customers_served++;
        total_customers_served++;
        barber->isBusy = 0;
        spinlock_unlock(stats_mutex);

        spinlock_unlock(register_mutex);
    }
    return NULL;
}


void* customer_thread(void* arg) {
    Customer *customer = (Customer *)arg;
    customer->arrival_time = time(NULL);
    spinlock_lock(chairs_mutex);
    if (free_chairs > 0) {
        free_chairs--;
        int barber_id = find_available_barber();
        if (barber_id != -1) {
            barbers[barber_id].isBusy = 1;
            customer->hasBeenSeated = 1;

            char log_msg[100];
            sprintf(log_msg, "Customer %d is getting a haircut from Barber %d", customer->id, barber_id);
            log_activity_to_file(log_msg);
            printf("%s\n", log_msg);

            send_message_to_client(customer->socket, "Your haircut is starting.\n");
        }
        spinlock_unlock(chairs_mutex);
    } else {
        spinlock_unlock(chairs_mutex);
        spinlock_lock(sofa_mutex);
        if (sofa_customers < SOFA_CAPACITY) {
            sofa_customers++;
            log_activity_to_file("Customer is waiting on the sofa");
            while (!customer->hasBeenSeated) {
                spinlock_unlock(sofa_mutex);
                usleep(100000);
                spinlock_lock(sofa_mutex);
            }
            sofa_customers--;
            spinlock_unlock(sofa_mutex);
        } else {
            log_activity_to_file("Customer left as there's no space");
            spinlock_unlock(sofa_mutex);
            free(customer);
            return NULL;
        }
    }

    spinlock_lock(stats_mutex);
    int wait_time = time(NULL) - customer->arrival_time;
    total_waiting_time += wait_time;
    spinlock_unlock(stats_mutex);

    free(customer);
    return NULL;
}


int random_range(int min, int max) {
    return rand() % (max - min + 1) + min;
}

int find_available_barber() {
    for (int i = 0; i < NUM_BARBERS; i++) {
        if (!barbers[i].isBusy) return i;
    }
    return -1;
}

void log_activity_to_file(const char* activity) {
    FILE *file = fopen("barbershop_activity_log.txt", "a");
    if (file == NULL) {
        perror("Error opening log file");
        return;
    }
    fprintf(file, "%s\n", activity);
    fclose(file);
}

void display_barbershop_status() {
    spinlock_lock(chairs_mutex);
    spinlock_lock(sofa_mutex);
    spinlock_lock(register_mutex);
    spinlock_lock(stats_mutex);

    printf("\n--- Barbershop Status ---\n");
    printf("Free Chairs: %d\n", free_chairs);
    printf("Customers on Sofa: %d\n", sofa_customers);
    printf("Customers Standing: %d\n", standing_customers);
    printf("Total Customers Served: %d\n", total_customers_served);
    printf("Average Wait Time: %.2f minutes\n", calculate_average_wait_time());

    for (int i = 0; i < NUM_BARBERS; i++) {
        printf("Barber %d - Status: %s, Customers Served: %d\n",
               barbers[i].id,
               barbers[i].isBusy ? "Busy" : "Idle",
               barbers[i].customers_served);
    }

    spinlock_unlock(stats_mutex);
    spinlock_unlock(register_mutex);
    spinlock_unlock(sofa_mutex);
    spinlock_unlock(chairs_mutex);
}

float calculate_average_wait_time() {
    spinlock_lock(stats_mutex);
    float average_wait = total_customers_served > 0 ? (float)total_waiting_time / total_customers_served : 0.0;
    spinlock_unlock(stats_mutex);
    return average_wait;
}

void process_customer_payment(Barber *barber) {
    spinlock_lock(register_mutex);

    char log_msg[100];
    sprintf(log_msg, "Barber %d is accepting payment", barber->id);
    printf("%s\n", log_msg);
    log_activity_to_file(log_msg);

    sleep(random_range(1, 3));

    spinlock_lock(stats_mutex);
    barber->customers_served++;
    total_customers_served++;
    spinlock_unlock(stats_mutex);

    barber->isBusy = 0;

    spinlock_unlock(register_mutex);
}

void cut_customer_hair(Barber *barber) {
    char log_msg[100];
    sprintf(log_msg, "Barber %d is cutting hair", barber->id);
    printf("%s\n", log_msg);
    log_activity_to_file(log_msg);

    sleep(random_range(7, 19));
}

void send_message_to_client(int client_sock, const char *message) {
    send(client_sock, message, strlen(message), 0);
}

