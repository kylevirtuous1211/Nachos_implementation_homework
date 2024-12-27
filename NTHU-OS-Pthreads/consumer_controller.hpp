#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "consumer.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include "transformer.hpp"

#ifndef CONSUMER_CONTROLLER
#define CONSUMER_CONTROLLER

class ConsumerController : public Thread {
public:
	// constructor
	ConsumerController(
		TSQueue<Item*>* worker_queue,
		TSQueue<Item*>* writer_queue,
		Transformer* transformer,
		int check_period,
		int low_threshold,
		int high_threshold
	);

	// destructor
	~ConsumerController();

	virtual void start();

private:
	std::vector<Consumer*> consumers;

	TSQueue<Item*>* worker_queue;
	TSQueue<Item*>* writer_queue;

	Transformer* transformer;

	// Check to scale down or scale up every check period in microseconds.
	int check_period;
	// When the number of items in the worker queue is lower than low_threshold,
	// the number of consumers scaled down by 1.
	int low_threshold;
	// When the number of items in the worker queue is higher than high_threshold,
	// the number of consumers scaled up by 1.
	int high_threshold;

	static void* process(void* arg);
};

// Implementation start

ConsumerController::ConsumerController(
	TSQueue<Item*>* worker_queue,
	TSQueue<Item*>* writer_queue,
	Transformer* transformer,
	int check_period,
	int low_threshold,
	int high_threshold
) : worker_queue(worker_queue),
	writer_queue(writer_queue),
	transformer(transformer),
	check_period(check_period),
	low_threshold(low_threshold),
	high_threshold(high_threshold) {
}

ConsumerController::~ConsumerController() {}

void ConsumerController::start() {
	// TODO: starts a ConsumerController thread
	pthread_create(&t, NULL, process, this);
}

void* ConsumerController::process(void* arg) {
    ConsumerController* controller = static_cast<ConsumerController*>(arg);

    while (true) {
        // Sleep for the specified check period
        usleep(controller->check_period);

        // Get the current size of the worker queue
        int current_size = controller->worker_queue->get_size();

        // Scale up: Add a new Consumer if queue size exceeds high_threshold
        if (current_size > controller->high_threshold) {
            Consumer* new_consumer = new Consumer(controller->worker_queue, controller->writer_queue, controller->transformer);
            controller->consumers.push_back(new_consumer);
            new_consumer->start();
            std::cout << "Added a new Consumer. Total Consumers from " << controller->consumers.size()-1 << " to " << controller->consumers.size() << std::endl;
        }
        // Scale down: Remove the last Consumer if queue size is below low_threshold
        else if (current_size < controller->low_threshold && controller->consumers.size() > 1) {
            Consumer* consumer_to_remove = controller->consumers.back();
            consumer_to_remove->cancel();
            controller->consumers.pop_back();
            delete consumer_to_remove;
            std::cout << "Removed a Consumer. Total Consumers from" << controller->consumers.size()+1 << " to " << controller->consumers.size() << std::endl;
        }
    }
    return nullptr;
}

#endif // CONSUMER_CONTROLLER_HPP
