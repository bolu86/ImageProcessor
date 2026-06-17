// ImageProcessor.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <chrono>
#include <iostream>
#include "Threading.h"

using namespace std::literals::chrono_literals;

int main()
{
	ThreadPool pool;

	auto f1 = pool.submit([]() {
		std::this_thread::sleep_for(1s);
		return 42;
		});

	auto f2 = pool.submit([](int x, int y) {
		return x + y;
		}, 10, 20);

	std::cout << "f1: " << f1.get() << "\n";  // blocks until ready
	std::cout << "f2: " << f2.get() << "\n";
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
