#include <fstream>
#include "thread.hpp"
#include "ts_queue.hpp"
#include "item.hpp"
#include <chrono>
#include <vector>
#ifndef WRITER_HPP
#define WRITER_HPP

class Writer : public Thread {
public:
	// constructor
	Writer(int expected_lines, std::string output_file, TSQueue<Item*>* output_queue);

	// destructor
	~Writer();

	virtual void start() override;
private:
	// the expected lines to write,
	// the writer thread finished after output expected lines of item
	int expected_lines;

	std::ofstream ofs;
	TSQueue<Item*> *output_queue;

	// the method for pthread to create a writer thread
	static void* process(void* arg);
	std::vector<long long> latencies;
};

// Implementation start

Writer::Writer(int expected_lines, std::string output_file, TSQueue<Item*>* output_queue)
	: expected_lines(expected_lines), output_queue(output_queue) {
	ofs = std::ofstream(output_file);
}

Writer::~Writer() {
	ofs.close();
}

void Writer::start() {
	// TD: starts a Writer thread
	// create and starts a new pthread and runs process function
	int ret = pthread_create(&t, NULL, process, this);
	if (ret != 0) {
		std::cerr << "pthread_create failed: " << ret << std::endl;
	}
}

void* Writer::process(void* arg) {
	// TD: implements the Writer's work
	// change the pointer to writer's pointer
	Writer* writer = static_cast<Writer*>(arg);
	if (!writer->output_queue || !writer->ofs.is_open()) {
		std::cerr << "output_queue or output file not open" << std::endl;
		return NULL;
	}
	// decrease expected lines and write to file
	while (writer -> expected_lines > 0) {
		Item* item = writer->output_queue->dequeue();
		// std::cout << *item << std::endl;
		if (writer->ofs.fail()) {
            std::cerr << "Failed to write item to file." << std::endl;
            break;
        }
		writer->ofs << *item;
		auto write_time = std::chrono::high_resolution_clock::now();
		auto latency = std::chrono::duration_cast<std::chrono::microseconds>(write_time - item->enqueue_time).count();
		writer->latencies.push_back(latency);
		--writer->expected_lines;
		// writer->ofs.flush();
		delete item;
	}
	if (!writer->latencies.empty()) {
		long long sum = 0;
		long long min_latency = writer->latencies[0];
		long long max_latency = writer->latencies[0];
		for (auto &latency : writer->latencies) {
			sum += latency;
			if (latency < min_latency) min_latency = latency;
			if (latency > max_latency) max_latency = latency;
		}
		std::cout << "Total items written: " << writer->latencies.size() << std::endl;
		std::cout << "Average latency: " << sum / writer->latencies.size() << "µs" << std::endl;
		std::cout << "Min latency: " << min_latency << "µs" << std::endl;
		std::cout << "Max latency: " << max_latency << "µs" << std::endl;
	}
	return NULL;
}

#endif // WRITER_HPP
