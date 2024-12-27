#include <pthread.h>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef PRODUCER_HPP
#define PRODUCER_HPP

class Producer : public Thread {
public:
	// constructor
	Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transfomrer);

	// destructor
	~Producer();

	virtual void start();
private:
	TSQueue<Item*>* input_queue;
	TSQueue<Item*>* worker_queue;

	Transformer* transformer;

	// the method for pthread to create a producer thread
	static void* process(void* arg);
};

Producer::Producer(TSQueue<Item*>* input_queue, TSQueue<Item*>* worker_queue, Transformer* transformer)
	: input_queue(input_queue), worker_queue(worker_queue), transformer(transformer) {
}

Producer::~Producer() {}

void Producer::start() {
	pthread_create(&t, NULL, process, this);
}

void* Producer::process(void* arg) {
    Producer* producer = static_cast<Producer*>(arg);
    while (true) {
        // Blocking dequeue call
        Item* rawItem = producer->input_queue->dequeue();
    
        // Transform the item
        auto val = producer->transformer->producer_transform(rawItem->opcode, rawItem->val);
        // std::cout << val << std::endl; // for debugging (looks good)
        // Enqueue the transformed item
        producer->worker_queue->enqueue(new Item(rawItem->key, val, rawItem->opcode));
        
        // Clean up the raw item
        delete rawItem;
    }
    return nullptr;
}

#endif // PRODUCER_HPP
