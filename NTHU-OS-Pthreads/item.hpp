#include <iostream>

#ifndef ITEM_HPP
#define ITEM_HPP
#include <chrono>

class Item {
public:
	Item();
	explicit Item(int key, unsigned long long val, char opcode);
	~Item();

	friend std::ostream& operator<<(std::ostream& os, const Item& item);
	friend std::istream& operator>>(std::istream& in, Item& item);

	int key;
	unsigned long long val;
	char opcode;
	std::chrono::high_resolution_clock::time_point enqueue_time;
};

// Implementation start

Item::Item() {}

Item::Item(int key, unsigned long long val, char opcode) :
	key(key), val(val), opcode(opcode), enqueue_time(std::chrono::high_resolution_clock::now()) {
}

Item::~Item() {}

std::istream& operator>>(std::istream& in, Item& item) {
	in >> item.key >> item.val >> item.opcode;
	return in;
}

std::ostream& operator<<(std::ostream& os, const Item& item) {
	os << item.key << ' ' << item.val << ' ' << item.opcode << '\n';
	return os;
}

#endif // ITEM_HPP
