/*
MIT License

Copyright (c) 2019 Meng Rao <raomeng1@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <time.h>

class TSCNS
{
public:
  // If you haven't calibrated tsc_ghz on this machine, set tsc_ghz as 0.0 and wait some time for calibration.
  // The wait time should be at least 1 second and the longer the more precise tsc_ghz calibrate can get.
  // We suggest that user waits as long as possible(more than 1 min) once, and save the resultant tsc_ghz
  // returned from calibrate() somewhere(e.g. config file) on this machine for future use.
  // Or you can cheat, see README and cheat.cc for details.
  //
  // If you have calibrated/cheated before on this machine as above, set tsc_ghz and skip calibration.
  //
  // One more thing: you can re-init TSCNS with the same tsc_ghz at later times if you want to re-sync with
  // system time in case of NTP or manual time changes.
  // re-init() is thread-safe with rdns(), because rdns() reads 2 member variables: ns_offset and tsc_ghz_inv,
  // re-init() won't change tsc_ghz_inv's value(although it writes to it anyway), only ns_offset can be changed
  // in an atomic way, thus thread safe.
  void init(double tsc_ghz = 0.0) {
    syncTime(base_tsc, base_ns);
    if (tsc_ghz <= 0.0) return;
    tsc_ghz_inv = 1.0 / tsc_ghz;
    adjustOffset();
  }

  double calibrate() {
    uint64_t delayed_tsc, delayed_ns;
    syncTime(delayed_tsc, delayed_ns);
    tsc_ghz_inv = (double)(int64_t)(delayed_ns - base_ns) / (int64_t)(delayed_tsc - base_tsc);
    adjustOffset();
    return 1.0 / tsc_ghz_inv;
  }

  uint64_t rdtsc() const {
    unsigned int dummy;
    return __builtin_ia32_rdtscp(&dummy);
  }

  // use below implementation if rdtscp is not supported by cpu
  // uint64_t rdtsc() const { return __builtin_ia32_rdtsc(); }

  uint64_t tsc2ns(uint64_t tsc) const { return ns_offset + (int64_t)((int64_t)tsc * tsc_ghz_inv); }

  uint64_t rdns() const { return tsc2ns(rdtsc()); }

  // If you want cross-platform, use std::chrono as below which incurs one more function call:
  // return std::chrono::high_resolution_clock::now().time_since_epoch().count();
  uint64_t rdsysns() const {
    timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
  }

  // For checking purposes, see test.cc
  uint64_t rdoffset() const { return ns_offset; }

private:
  // Linux kernel sync time by finding the first try with tsc diff < 50000
  // We do better: we find the try with the mininum tsc diff
  void syncTime(uint64_t& tsc, uint64_t& ns) {
    const int N = 10;
    uint64_t tscs[N + 1];
    uint64_t nses[N + 1];

    tscs[0] = rdtsc();
    for (int i = 1; i <= N; i++) {
      nses[i] = rdsysns();
      tscs[i] = rdtsc();
    }

    int best = 1;
    for (int i = 2; i <= N; i++) {
      if (tscs[i] - tscs[i - 1] < tscs[best] - tscs[best - 1]) best = i;
    }
    tsc = (tscs[best] + tscs[best - 1]) >> 1;
    ns = nses[best];
  }

  void adjustOffset() { ns_offset = base_ns - (int64_t)((int64_t)base_tsc * tsc_ghz_inv); }

  alignas(64) double tsc_ghz_inv = 1.0; // make sure tsc_ghz_inv and ns_offset are on the same cache line
  uint64_t ns_offset = 0;
  uint64_t base_tsc = 0;
  uint64_t base_ns = 0;
};
