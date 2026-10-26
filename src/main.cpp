#include <mumbleUtils.hpp>

int main(int argc, char* argv[])
{
	std::string testSpeech = "Hello there! Is this... ENOUGH?";

	auto data = mumble::renderLine(testSpeech, mumble::preset::man);
	std::cout << "First result was returned with: " << mumble::writeWavToFile(data, mumble::SAMPLE_RATE, "first.wav") << "\n";

	auto data2 = mumble::renderLine("There is more where that came from! Young GRASSHOPPER!", mumble::preset::child);
	std::cout << "Second result was returned with: " << mumble::writeWavToFile(data2) << "\n";
	
	auto data3 = mumble::renderLine("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.", mumble::preset::woman);
	std::cout << "Second result was returned with: " << mumble::writeWavToFile(data3, mumble::SAMPLE_RATE, "lorem.wav") << "\n";

	return 0;
}