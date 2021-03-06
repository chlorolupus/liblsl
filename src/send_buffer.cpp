#include "send_buffer.h"
#include "consumer_queue.h"
#include <memory>


using namespace lsl;

/**
 * Add a new consumer to the send buffer.
 * Each consumer will get all samples (although the oldest samples will be dropped when the buffer
 *capacity is overrun).
 * @param max_buffered If non-zero, the queue size for this consumer will be constrained to be no
 *larger than this value. Note that the actual queue size will also never exceed the max_capacity of
 *the send_buffer (so this is a global limit).
 * @return Shared pointer to the newly created queue.
 */
consumer_queue_p send_buffer::new_consumer(int max_buffered) {
	max_buffered = max_buffered ? std::min(max_buffered, max_capacity_) : max_capacity_;
	return std::make_shared<consumer_queue>(max_buffered, shared_from_this());
}


/**
 * Push a sample onto the send buffer.
 * Will subsequently be seen by all consumers.
 */
void send_buffer::push_sample(const sample_p &s) {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	for (auto &consumer : consumers_) consumer->push_sample(s);
}


/// Registered a new consumer.
void send_buffer::register_consumer(consumer_queue *q) {
	{
		std::lock_guard<std::mutex> lock(consumers_mut_);
		consumers_.insert(q);
	}
	some_registered_.notify_all();
}

/// Unregister a previously registered consumer.
void send_buffer::unregister_consumer(consumer_queue *q) {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	consumers_.erase(q);
}

/// Check whether there currently are consumers.
bool send_buffer::have_consumers() {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	return some_registered();
}

/// Wait until some consumers are present.
bool send_buffer::wait_for_consumers(double timeout) {
	std::unique_lock<std::mutex> lock(consumers_mut_);
	return some_registered_.wait_for(
		lock, std::chrono::duration<double>(timeout), [this]() { return some_registered(); });
}
