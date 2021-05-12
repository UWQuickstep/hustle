#include "tatp_hustle_benchmark.h"
#include <iostream>
#include <fstream>


int main(int argc, char **argv) {
  int n_workers = 3;
  int warmup_duration = 5;
  int measurement_duration = 15;
  int n_rows = 100000;
  char *out_file;
  if (argc == 6) {
    n_workers = std::stoi(argv[1]);
    warmup_duration = std::stoi(argv[2]);
    measurement_duration = std::stoi(argv[3]);
    n_rows = std::stoi(argv[4]);
    out_file = argv[5];
  }

   std::cout << "n_workers: " << n_workers << "\n"
	     << "warmup_duration: " << warmup_duration << "\n"
             << "measurement_duration: " << measurement_duration << "\n"
	     << "n_rows: " << n_rows << "\n";

  txbench::TATPHustleBenchmark benchmark(n_workers, warmup_duration,
                                        measurement_duration, n_rows);
  double tps = benchmark.run();
  std::ofstream outfile;

  std::cout << "n_workers : " << n_workers <<" tps : " << tps << std::endl;

    outfile.open(out_file, std::ios_base::app);
    outfile << std::to_string(n_workers) << "," << std::to_string(tps) << std::endl;
    hustle::profiler.summarizeToStream(std::cout);
}
