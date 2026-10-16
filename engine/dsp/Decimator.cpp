#include "Decimator.h"

#include <algorithm>
#include <cmath>

#include "DspMath.h"

namespace p10 {

namespace {
double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-17 * sum) break;
    }
    return sum;
}
}  // namespace

void Decimator::configure(int factor, double outputRate) {
    factor_ = std::max(1, factor);
    h_.clear();
    if (factor_ == 1) {
        h_ = {1.0};
    } else {
        const double fsIn = outputRate * factor_;
        const double pass = std::min(20000.0, 0.45 * outputRate);
        const double stop = outputRate - pass;
        const double atten = 115.0;
        const double beta = 0.1102 * (atten - 8.7);
        const double transition = (stop - pass) / fsIn;  // cycles/sample
        int n = static_cast<int>(std::ceil((atten - 8.0) / (2.285 * kTwoPi * transition))) + 1;
        n |= 1;  // odd length, integer group delay
        const double fc = 0.5 * (pass + stop) / fsIn;
        h_.resize(n);
        const double m = 0.5 * (n - 1);
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double t = i - m;
            const double x = 2.0 * fc * t;
            const double sinc = std::fabs(x) < 1e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double r = t / m;
            const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
            h_[i] = 2.0 * fc * sinc * w;
            sum += h_[i];
        }
        for (double& v : h_) v /= sum;
    }
    hist_.assign(2 * h_.size(), 0.0);
    pos_ = 0;
}

void Decimator::reset() {
    std::fill(hist_.begin(), hist_.end(), 0.0);
    pos_ = 0;
}

void Decimator::process(const double* in, double* out, int n) {
    const int len = static_cast<int>(h_.size());
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < factor_; ++j) {
            pos_ = (pos_ == 0) ? len - 1 : pos_ - 1;
            const double v = in[i * factor_ + j];
            hist_[pos_] = v;
            hist_[pos_ + len] = v;
        }
        const double* x = &hist_[pos_];
        double acc = 0.0;
        for (int k = 0; k < len; ++k) acc += h_[k] * x[k];
        out[i] = acc;
    }
}

}  // namespace p10
