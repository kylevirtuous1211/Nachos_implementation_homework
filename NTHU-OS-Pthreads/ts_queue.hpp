#include <pthread.h>

#ifndef TS_QUEUE_HPP
#define TS_QUEUE_HPP

#define DEFAULT_BUFFER_SIZE 200

template <class T>
class TSQueue {
public:
	// constructor
	TSQueue();

	explicit TSQueue(int max_buffer_size);

	// destructor
	~TSQueue();

	// add an element to the end of the queue
	void enqueue(T item);

	// remove and return the first element of the queue
	T dequeue();

	// return the number of elements in the queue
	int get_size();
private:
	// the maximum buffer size
	int buffer_size;
	// the buffer containing values of the queue
	T* buffer;
	// the current size of the buffer
	int size;
	// the index of first item in the queue
	int head;
	// the index of last item in the queue
	int tail;

	// pthread mutex lock
	pthread_mutex_t mutex;
	// pthread conditional variable
	pthread_cond_t cond_enqueue, cond_dequeue;
};

// Implementation start

template <class T>
TSQueue<T>::TSQueue() : TSQueue(DEFAULT_BUFFER_SIZE) {
}

template <class T>
TSQueue<T>::TSQueue(int buffer_size) : buffer_size(buffer_size) {
	// TD: implements TSQueue constructor
	buffer = new T[buffer_size];
	size = 0;
	head = buffer_size - 1;
	tail = 0;
	// initialize pthread mutex lock
	pthread_mutex_init(&mutex, NULL);
	// initialize pthread conditional variable
	pthread_cond_init(&cond_enqueue, NULL);
	pthread_cond_init(&cond_dequeue, NULL);	
}

template <class T>
TSQueue<T>::~TSQueue() {
	// TD: implenents TSQueue destructor
	delete [] buffer;
	pthread_cond_destroy(&cond_dequeue);
	pthread_cond_destroy(&cond_enqueue);
	pthread_mutex_destroy(&mutex);
}

template <class T>
void TSQueue<T>::enqueue(T item) {
	// TD: enqueues an element to the end of the queue
	pthread_mutex_lock(&mutex);
	// queue is full => blocked 
	while (size == buffer_size) {
		pthread_cond_wait(&cond_enqueue, &mutex);
	}
	// has space to enqueue, enqueue from tail
	buffer[tail] = item;
	tail = (tail + 1) % buffer_size;
	size++;
	pthread_cond_signal(&cond_dequeue);	
	pthread_mutex_unlock(&mutex);
}

template <class T>
T TSQueue<T>::dequeue() {
	pthread_mutex_lock(&mutex);
	// queue is full => blocked 
	while (size == 0) {
		pthread_cond_wait(&cond_dequeue, &mutex);
	}
	// has item at head, dequeue from head
	head = (head + 1) % buffer_size;
	T item = buffer[head];
	size--;
	pthread_cond_signal(&cond_enqueue);
	pthread_mutex_unlock(&mutex);
	return item;
}

template <class T>
int TSQueue<T>::get_size() {
	// TD: returns the size of the queue
	return size;
}

#endif // TS_QUEUE_HPP
