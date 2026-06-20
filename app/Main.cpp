#include <chrono>
#include "threading\ThreadPool.h"

using namespace std::literals::chrono_literals;

int main()
{
	std::size_t numThreads = std::thread::hardware_concurrency();
	std::cout << "Cores: " << numThreads << std::endl;

	//Pipeline pipeline(collectImagePaths("input_folder"), "output_folder", /*queueCapacity=*/8);
	//pipeline.run();

	ThreadPool pool(numThreads, 10);

	auto f1 = pool.submit([]() {
		std::this_thread::sleep_for(1s);
		return 42;
		});

	auto f2 = pool.submit([](int x, int y) {
		return x + y;
		}, 10, 20);

	std::cout << "Result: " << f1->get() << "\n";
	std::cout << "Result: " << f2->get() << "\n";
}