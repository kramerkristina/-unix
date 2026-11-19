#include <iostream>
#include <pthread.h>
#include <unistd.h>

class Event {
public:
    int num;
};

bool ready = false;
Event *ptr = NULL;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void *provider(void *arg)
{
    int i = 0;
    while (true) {
        sleep(1);
        pthread_mutex_lock(&mtx);
        if (ready == false) {
            i++;
            ptr = new Event;
            ptr->num = i;
            ready = true;
            std::cout << "Поставщик отправил событие " << ptr->num << std::endl;
            pthread_cond_signal(&cv);
        }
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

void *consumer(void *arg)
{
    while (true) {
        pthread_mutex_lock(&mtx);
        while (ready == false)
            pthread_cond_wait(&cv, &mtx);
        Event *e = ptr;
        ptr = NULL;
        ready = false;
        std::cout << "Потребитель получил событие " << e->num << std::endl;
        delete e;
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

int main()
{
    pthread_t prov;
    pthread_t cons;
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_create(&prov, NULL, provider, NULL);
    pthread_join(prov, NULL);
    return 0;
}
