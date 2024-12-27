#include <assert.h>
#include <stdlib.h>
#include "ts_queue.hpp"
#include "item.hpp"
#include "reader.hpp"
#include "writer.hpp"
#include "producer.hpp"
#include "consumer_controller.hpp"
#include <chrono>
using namespace std;

#define READER_QUEUE_SIZE 200
#define WORKER_QUEUE_SIZE 200
#define WRITER_QUEUE_SIZE 4000
#define CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE 20
#define CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE 80
#define CONSUMER_CONTROLLER_CHECK_PERIOD 1000000

int main(int argc, char** argv) {
	auto start_time = std::chrono::high_resolution_clock::now();
	assert(argc == 4);

	int expected_lines = atoi(argv[1]);
	std::string input_file_name(argv[2]);
	std::string output_file_name(argv[3]);

	// TODO: implements main function
	// create queues
	TSQueue<Item*>* input_queue = new TSQueue<Item*>(READER_QUEUE_SIZE);
	TSQueue<Item*>* worker_queue = new TSQueue<Item*>(WORKER_QUEUE_SIZE);
	TSQueue<Item*>* output_queue = new TSQueue<Item*>(WRITER_QUEUE_SIZE);
	// start reader thread
	Reader* reader = new Reader(expected_lines, input_file_name, input_queue);
	reader->start();
	// start transformer thread
	Transformer* transformer = new Transformer();
	// start 4 producer threads
	std::vector<Producer*> producers;

	for (int i = 0; i < 4; i++) {
		Producer* producer = new Producer(input_queue, worker_queue, transformer);
		producer->start();
		producers.push_back(producer);
	}
	Writer* writer = new Writer(expected_lines, output_file_name, output_queue);
	writer->start();

	ConsumerController* consumer_controller = new ConsumerController(
		worker_queue,
		output_queue,
		transformer,
		CONSUMER_CONTROLLER_CHECK_PERIOD,
		WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_LOW_THRESHOLD_PERCENTAGE / 100,
		WORKER_QUEUE_SIZE * CONSUMER_CONTROLLER_HIGH_THRESHOLD_PERCENTAGE / 100
	);
	consumer_controller->start();

	reader->join(); // wait for reader thread to complete
	// cout << "reader thread complete" << endl;
	writer->join(); // wait for writer thread to complete
	// cout << "writer thread complete" << endl;
	// cout << "producer threads complete" << endl;

	// delete threads
	delete reader;
	delete writer;
	delete consumer_controller;
	delete transformer;	
	for (auto& producer : producers) {
		delete producer;
	}

	// delete queues
	delete input_queue;
	delete worker_queue;
	delete output_queue;
	// cout << "all threads and queues complete" << endl;
	// ends after reader and writer thread complete
	auto end_time = std::chrono::high_resolution_clock::now();
	std::cout << "Total Execution time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms" << std::endl;
	return 0;
}
