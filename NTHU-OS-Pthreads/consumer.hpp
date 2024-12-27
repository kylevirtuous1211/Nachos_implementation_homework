#include <pthread.h>
#include <stdio.h>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef CONSUMER_HPP
#define CONSUMER_HPP

class Consumer : public Thread {
public:
	// constructor
	Consumer(TSQueue<Item*>* worker_queue, TSQueue<Item*>* output_queue, Transformer* transformer);

	// destructor
	~Consumer();

	virtual void start() override;

	virtual int cancel() override;
private:
	TSQueue<Item*>* worker_queue;
	TSQueue<Item*>* output_queue;

	Transformer* transformer;

	bool is_cancel;

	// the method for pthread to create a consumer thread
	static void* process(void* arg);
};

Consumer::Consumer(TSQueue<Item*>* worker_queue, TSQueue<Item*>* output_queue, Transformer* transformer)
	: worker_queue(worker_queue), output_queue(output_queue), transformer(transformer) {
	is_cancel = false;
}

Consumer::~Consumer() {}

void Consumer::start() {
	pthread_create(&t, nullptr, process, this);
}

int Consumer::cancel() {
	// TD: cancels the consumer thread
	is_cancel = true;
	return pthread_cancel(t);
}

void* Consumer::process(void* arg) {
	Consumer* consumer = (Consumer*)arg;

	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

	while (!consumer->is_cancel) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

		Item* item = consumer->worker_queue->dequeue();

		unsigned long long val = consumer->transformer->consumer_transform(item->opcode, item->val);
		// std::cout << val << std::endl; // for debugging (looks good)
		consumer->output_queue->enqueue(new Item(item->key, val, item->opcode));
		delete item;

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
	}
	return nullptr;
}

#endif // CONSUMER_HPP
