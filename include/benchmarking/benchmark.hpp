#pragma once
#include <chrono>
#include <limits>
#include <numeric>
#include <chacha20_poly1305.hpp>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <sched.h>
#endif

namespace Benchmarking {
	void set_high_priority() {
#if defined(_WIN32) || defined(_WIN64)
		DWORD_PTR cpuset = 0;

		cpuset |= (1ULL << 0);

		HANDLE currentThread = GetCurrentThread();
		SetThreadAffinityMask(currentThread, cpuset);
#elif defined(__linux__) || defined(__GLIBC__)
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(0, &cpuset);
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
	}

	inline uint64_t read_cycles() {
		_mm_lfence();
		unsigned int ui;
		return __rdtscp(&ui);
	}

	struct PerformanceResults {
		double average_throughput = 0.0;
		double best_throughput = 0.0;
		double worst_throughput = std::numeric_limits<double>::max();
		double throughput_amplitude = 0.0;
		double throughput_iqr = 0.0;
		double average_cpb = 0.0;
		std::chrono::duration<double> biggest_time = std::chrono::duration<double>::zero();
		std::chrono::duration<double> smallest_time = std::chrono::duration<double>::max();
		std::chrono::duration<double> average_time = std::chrono::duration<double>::zero();
		std::chrono::duration<double> time_amplitude = std::chrono::duration<double>::zero();
		std::chrono::duration<double> time_iqr = std::chrono::duration<double>::zero();

		void print(const std::string title) const {
			const int label_w = 25;
			const int value_w = 15;

			std::cout << "\n=======================================================" << std::endl;
			std::cout << " " << title << std::endl;
			std::cout << "=======================================================" << std::endl;

			std::cout << std::fixed << std::setprecision(4);

			std::cout << "[ THROUGHPUT ]" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Best:" << std::right << std::setw(value_w) << best_throughput << "MB/s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Worst:" << std::right << std::setw(value_w) << (worst_throughput == std::numeric_limits<double>::max() ? 0.0 : worst_throughput) << "MB/s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Average:" << std::right << std::setw(value_w) << average_throughput << "MB/s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Amplitude:" << std::right << std::setw(value_w) << throughput_amplitude << "MB/s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  IQR:" << std::right << std::setw(value_w) << throughput_iqr << "MB/s" << std::endl;

			std::cout << "\n[ TIME (seconds) ]" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Smallest:" << std::right << std::setw(value_w) << smallest_time.count() << "s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Biggest:" << std::right << std::setw(value_w) << biggest_time.count() << "s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Average:" << std::right << std::setw(value_w) << average_time.count() << "s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  Amplitude:" << std::right << std::setw(value_w) << time_amplitude.count() << "s" << std::endl;
			std::cout << std::left << std::setw(label_w) << "  IQR:" << std::right << std::setw(value_w) << time_iqr.count() << "s" << std::endl;

        	std::cout << "[ EFFICIENCY ]" << std::endl;
        	std::cout << std::left << std::setw(25) << "  Average CPB:" << std::right << std::setw(15) << average_cpb << " c/B" << std::endl;
			std::cout << "=======================================================\n" << std::endl;
		}
	};

	struct PerformanceMetric {
	private:
		std::vector<std::chrono::duration<double>> times;
		std::vector<double> throughputs;
		std::vector<uint64_t> total_cycles;
		double bytes_per_run;
	public:
		PerformanceMetric(size_t reserve_size, double bytes_): bytes_per_run(bytes_) {
			times.reserve(reserve_size);
			throughputs.reserve(reserve_size);
			total_cycles.reserve(reserve_size);
		}

		void pushMetrics(std::chrono::duration<double> time, double throughput, double cpb) {
			times.push_back(time);
			throughputs.push_back(throughput);
			total_cycles.push_back(cpb);
		}

		PerformanceResults finish() {
			if (throughputs.empty()) {
				throw std::runtime_error("No benchmarks to evaluate");
			}

			PerformanceResults r;
			const double n = static_cast<double>(throughputs.size());

			double throughput_sum = std::accumulate(throughputs.begin(), throughputs.end(), 0.0);
			r.average_throughput = throughput_sum / n;

			auto time_sum = std::accumulate(times.begin(), times.end(), std::chrono::duration<double>::zero());
			r.average_time = time_sum / n;

			auto [min_t_it, max_t_it] = std::minmax_element(throughputs.begin(), throughputs.end());
			r.worst_throughput = *min_t_it;
			r.best_throughput = *max_t_it;
			r.throughput_amplitude = r.best_throughput - r.worst_throughput;

			auto [min_time_it, max_time_it] = std::minmax_element(times.begin(), times.end());
			r.smallest_time = *min_time_it;
			r.biggest_time = *max_time_it;
			r.time_amplitude = r.biggest_time - r.smallest_time;

			auto get_iqr = [](auto& vec) {
				auto q1_it = vec.begin() + vec.size() / 4;
				auto q3_it = vec.begin() + 3 * vec.size() / 4;
				std::nth_element(vec.begin(), q1_it, vec.end());
				std::nth_element(q1_it, q3_it, vec.end());
				return *q3_it - *q1_it;
			};

			r.throughput_iqr = get_iqr(throughputs);
			r.time_iqr = get_iqr(times);

			double avg_cycles = std::accumulate(total_cycles.begin(), total_cycles.end(), 0.0) / total_cycles.size();
        	r.average_cpb = avg_cycles / static_cast<double>(bytes_per_run);

			return r;
		}
	};

	static size_t constexpr WARMUP_ITERATIONS = 30;
	static size_t constexpr WARMUP_DATA_SIZE = 1024 * 1024 * 10; // 5 MB

	static size_t constexpr TEST_ITERATIONS = 30;
	static size_t constexpr TEST_DATA_SIZE = 1024 * 1024 * 8; // 8 MB

	static size_t constexpr KB = 1024;

	void run_chacha20_tests(const size_t data_size, const size_t rounds, PerformanceMetric& metrics, ChaCha20& test, bool verbose=false) {
		std::vector<uint8_t> plaintext(data_size, 0xAA);
		std::vector<uint8_t> ciphertext(data_size);
		for (size_t i = 0; i < rounds; i++) {
			test.set_counter(0);

			uint64_t start_cycles = read_cycles();

			auto start = std::chrono::steady_clock::now();
			test.process(plaintext.data(), ciphertext.data(), data_size);

			uint64_t end_cycles = read_cycles();;
        	auto end = std::chrono::steady_clock::now();

			std::chrono::duration<double> duration = end - start;
			uint64_t total_cycles = end_cycles - start_cycles;
			double throughput_mbps = (data_size / (1024.0 * 1024.0)) / duration.count();
			double cpb = static_cast<double>(total_cycles) / data_size;

			if (verbose) {
				std::cout << std::endl << "Test " << (i + 1) << std::endl;
				std::cout << "Took: " << duration.count() << "s" << std::endl;
				std::cout << "Throughput: " << throughput_mbps << "MB/s" << std::endl;
				std::cout << "CPB: " << cpb << std::endl;
				std::cout << "========================================" << std::endl;
			}

			metrics.pushMetrics(duration, throughput_mbps, total_cycles);
		}
	}

	void run_chacha_aead_tests(const size_t data_size, const size_t rounds, PerformanceMetric& enc_metrics, PerformanceMetric& dec_metrics, ChaCha20& test, bool verbose=false) {
		std::vector<uint8_t> plaintext(data_size, 0xAA);
		std::vector<uint8_t> ciphertext(data_size);
		std::vector<uint8_t> aad(16, 0x03);
		uint8_t tag[16];

		for (size_t i = 0; i < rounds; i++) {
			if(verbose) std::cout << "\nTest " << (i + 1) << "\n" << std::endl;

			// --- ENCRYPTION ---
			uint64_t start_cycles = read_cycles();
			auto start = std::chrono::steady_clock::now();

			ChaCha20_Poly1305::encrypt(test, plaintext.data(), plaintext.size(), aad.data(), aad.size(), ciphertext.data(), tag);

			uint64_t end_cycles = read_cycles();
			auto end = std::chrono::steady_clock::now();

			uint64_t diff_cycles = end_cycles - start_cycles;
			std::chrono::duration<double> duration = end - start;
			double throughput_mbps = (data_size / (1024.0 * 1024.0)) / duration.count();
			double cpb = static_cast<double>(diff_cycles) / data_size;

			if (verbose) {
				std::cout << "Encryption" << std::endl;
				std::cout << "Took: " << duration.count() << "s" << std::endl;
				std::cout << "Throughput: " << throughput_mbps << " MB/s" << std::endl;
				std::cout << "CPB: " << cpb << " cycles/byte\n" << std::endl;
			}
			
			enc_metrics.pushMetrics(duration, throughput_mbps, diff_cycles);

			// --- DECRYPTION ---
			start_cycles = read_cycles();
			start = std::chrono::steady_clock::now();

			ChaCha20_Poly1305::decrypt(test, ciphertext.data(), ciphertext.size(), aad.data(), aad.size(), tag, ciphertext.data());

			end_cycles = read_cycles();
			end = std::chrono::steady_clock::now();

			diff_cycles = end_cycles - start_cycles;
			duration = end - start;
			throughput_mbps = (data_size / (1024.0 * 1024.0)) / duration.count();
			cpb = static_cast<double>(diff_cycles) / data_size;

			if (verbose) {
				std::cout << "Decryption" << std::endl;
				std::cout << "Took: " << duration.count() << "s" << std::endl;
				std::cout << "Throughput: " << throughput_mbps << " MB/s" << std::endl;
				std::cout << "CPB: " << cpb << " cycles/byte\n" << std::endl;
				std::cout << "========================================" << std::endl;
			}

			dec_metrics.pushMetrics(duration, throughput_mbps, diff_cycles);
		}
	}


	void test_chacha20_correctness(ChaCha20& c) {
		c.set_counter(0);
		std::string test_str = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. Nullam dictum felis eu pede mollis pretium. Integer tincidunt.";
		std::vector<uint8_t> str_bytes = CryptoHelper::string_to_bytes(test_str);
		std::vector<uint8_t> ciphertext(str_bytes.size());

		c.process(str_bytes.data(), ciphertext.data(), str_bytes.size());
		c.set_counter(0);
		c.process(ciphertext.data(), str_bytes.data(), str_bytes.size());

		std::string rebuilt_str = CryptoHelper::bytes_to_string(str_bytes);

		if (rebuilt_str != test_str) {
			throw std::runtime_error("Encryption/Decryption don't match");
		}

		std::cout << "Encryption/Decryption match" << std::endl;
	}

	void test_chacha20poly1305_correctness(ChaCha20& c) {
		std::string test_str = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. Nullam dictum felis eu pede mollis pretium. Integer tincidunt.";
		std::string aad_str = "thisisaadstringidkwhattoputhere";
		std::vector<uint8_t> str_bytes = CryptoHelper::string_to_bytes(test_str);
		std::vector<uint8_t> aad_bytes = CryptoHelper::string_to_bytes(aad_str);
		std::vector<uint8_t> ciphertext(str_bytes.size());
		uint8_t tag[16];

		ChaCha20_Poly1305::encrypt(c, str_bytes.data(), str_bytes.size(), aad_bytes.data(), aad_bytes.size(), ciphertext.data(), tag);

		try {
			ChaCha20_Poly1305::decrypt(c, ciphertext.data(), ciphertext.size(), aad_bytes.data(), aad_bytes.size(), tag, str_bytes.data());
		}
		catch (const std::exception& e) {
			throw std::runtime_error("Poly1305 couldn't recreate tag");
		}

		std::cout << "Tag generated correctly" << std::endl;

		std::string rebuilt_str = CryptoHelper::bytes_to_string(str_bytes);

		if (rebuilt_str != test_str) {
			throw std::runtime_error("Encryption/Decryption don't match");
		}

		std::cout << "Encryption/Decryption match" << std::endl;
	}

	void test_chacha20(bool verbose=false) {
		uint32_t key[8] = {
			0xa9, 0xf1, 0xb3, 0x39,
			0x04, 0xff, 0xa1, 0xb7
		};

		uint32_t nonce[3] = { 0xe5, 0xa3, 0x88 };

		ChaCha20 test(key, nonce);

		uint8_t* warm_in = new uint8_t[WARMUP_DATA_SIZE];
		uint8_t* warm_out = new uint8_t[WARMUP_DATA_SIZE];

		std::memset(warm_in, 0xAA, WARMUP_DATA_SIZE);

		for (size_t i = 0; i < WARMUP_ITERATIONS; i++) {
			test.process(warm_in, warm_out, WARMUP_DATA_SIZE);
		}

		delete[] warm_in;
		delete[] warm_out;

		std::cout << "Warmup done" << std::endl;

		PerformanceMetric metric(TEST_ITERATIONS, TEST_DATA_SIZE);

		run_chacha20_tests(TEST_DATA_SIZE, TEST_ITERATIONS, metric, test, verbose);

		PerformanceResults r = metric.finish();

		r.print("ChaCha20 performance tests");

		test_chacha20_correctness(test);
	}

	void test_aead(bool verbose = false) {
		uint32_t key[8] = {
			0xa9, 0xf1, 0xb3, 0x39,
			0x04, 0xff, 0xa1, 0xb7
		};

		uint32_t nonce[3] = { 0xe5, 0xa3, 0x88 };

		ChaCha20 test(key, nonce);

		{

			std::vector<uint8_t> warm_in(WARMUP_DATA_SIZE, 0xAA);
			std::vector<uint8_t> warm_out(WARMUP_DATA_SIZE);
			std::vector<uint8_t> aad(KB, 0xF8); // 1 KB
			uint8_t tag[16];

			for (size_t i = 0; i < WARMUP_ITERATIONS; i++) {
				ChaCha20_Poly1305::encrypt(test, warm_in.data(), WARMUP_DATA_SIZE, aad.data(), KB, warm_out.data(), tag);
				ChaCha20_Poly1305::decrypt(test, warm_out.data(), WARMUP_DATA_SIZE, aad.data(), KB, tag, warm_in.data());
			}
		}

		PerformanceMetric enc_metric(TEST_ITERATIONS, TEST_DATA_SIZE);
		PerformanceMetric dec_metric(TEST_ITERATIONS, TEST_DATA_SIZE);

		run_chacha_aead_tests(TEST_DATA_SIZE, TEST_ITERATIONS, enc_metric, dec_metric, test, verbose);

		PerformanceResults enc_res = enc_metric.finish();
		PerformanceResults dec_res = dec_metric.finish();

		enc_res.print("ChaCha20-Poly1305 encryption metrics");
		dec_res.print("ChaCha20-Poly1305 decryption metrics");

		test_chacha20poly1305_correctness(test);
	}
}