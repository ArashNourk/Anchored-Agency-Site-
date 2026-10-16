// Kaiser-windowed FIR decimator from the voice oversampled rate to the output
// rate. Passband edge 20 kHz (or 0.45 fs if lower); stopband begins at
// fs_out - passband edge, so anything that folds lands above the passband
// edge. Designed for >= 110 dB stopband attenuation.
#pragma once

#include <vector>

namespace p10 {

class Decimator {
public:
    void configure(int factor, double outputRate);
    void reset();
    // in: factor * n samples, out: n samples.
    void process(const double* in, double* out, int n);
    int factor() const { return factor_; }
    int taps() const { return static_cast<int>(h_.size()); }
    double latencyOutputSamples() const { return 0.5 * (h_.size() - 1) / factor_; }

private:
    int factor_ = 1;
    std::vector<double> h_;
    std::vector<double> hist_;  // doubled circular history
    int pos_ = 0;
};

}  // namespace p10
