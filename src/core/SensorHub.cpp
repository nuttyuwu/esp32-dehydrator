#include "core/SensorHub.h"

#include <algorithm>

namespace dh {

namespace {
constexpr float kEmaAlpha = 0.3f;

float median3(float a, float b, float c) {
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}
}  // namespace

float SensorHub::Channel::push(float v) {
    buf_[idx_] = v;
    idx_ = static_cast<uint8_t>((idx_ + 1) % 3);
    if (n_ < 3) n_++;

    const float med = (n_ < 3) ? v : median3(buf_[0], buf_[1], buf_[2]);
    if (!init_) {
        ema_ = med;
        init_ = true;
    } else {
        ema_ = kEmaAlpha * med + (1.0f - kEmaAlpha) * ema_;
    }
    return ema_;
}

void SensorHub::begin(uint32_t nowMs) {
    fTBot_.reset();
    fRhBot_.reset();
    fTTop_.reset();
    fRhTop_.reset();
    bot_ = Reading{};
    top_ = Reading{};
    rawBot_ = Reading{};
    rawTop_ = Reading{};
    // Start the clock now, otherwise the first tick looks like a 10 s outage.
    lastValidMs_ = nowMs;
}

void SensorHub::update(const Reading& bot, const Reading& top, const Config& cfg, uint32_t nowMs) {
    rawBot_ = bot;
    rawTop_ = top;
    if (bot.valid) {
        rawBot_.t += cfg.offBotT;
        rawBot_.rh += cfg.offBotRh;
    }
    if (top.valid) {
        rawTop_.t += cfg.offTopT;
        rawTop_.rh += cfg.offTopRh;
    }

    if (bot.valid) {
        bot_.t = fTBot_.push(bot.t + cfg.offBotT);
        bot_.rh = fRhBot_.push(bot.rh + cfg.offBotRh);
        bot_.valid = true;
    } else {
        bot_.valid = false;
        fTBot_.reset();
        fRhBot_.reset();
    }

    if (top.valid) {
        top_.t = fTTop_.push(top.t + cfg.offTopT);
        top_.rh = fRhTop_.push(top.rh + cfg.offTopRh);
        top_.valid = true;
    } else {
        top_.valid = false;
        fTTop_.reset();
        fRhTop_.reset();
    }

    if (bot_.valid || top_.valid) lastValidMs_ = nowMs;
}

Reading SensorHub::control() const {
    if (bot_.valid) return bot_;
    if (top_.valid) return top_;
    return Reading{};
}

uint32_t SensorHub::invalidForMs(uint32_t nowMs) const {
    if (anyValid()) return 0;
    return nowMs - lastValidMs_;   // unsigned wrap is correct here
}

}  // namespace dh
