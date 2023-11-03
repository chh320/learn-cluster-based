#pragma once

#include <chrono>
#include <string>
#include <iostream>

namespace Util {
	class Timer final {
	public:
		Timer() {
			reset();
		}

		void reset() {
			timeStart = std::chrono::high_resolution_clock::now();
		}

		double timeDuration() {
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - timeStart).count();
		}


		void log(const std::string& content) {
			std::cerr << content << " ... " << timeDuration() * 0.000001<< " s\n";
		}

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> timeStart;
	};
}