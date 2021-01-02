#ifndef TXBENCH__WORKER_H
#define TXBENCH__WORKER_H

#include <atomic>

namespace txbench {

class Worker {
public:
  virtual ~Worker() = default;

  virtual void run(std::atomic_bool &terminate,
                   std::atomic_int &commit_count) = 0;
  virtual void print(int commit_count) = 0;

  virtual double getTime1() = 0;
  virtual double getTime2() = 0;
  virtual double getTime3() = 0;
  virtual double getTime4() = 0;
  virtual double getTime5() = 0;
  virtual double getTime6() = 0;
  virtual double getTime7() = 0;
};

} // namespace txbench

#endif // TXBENCH__WORKER_H
