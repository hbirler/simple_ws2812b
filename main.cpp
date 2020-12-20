#include <iostream>
#include <chrono>
#include <cmath>
#include <array>
#include <utility>
#include <time.h>
#include <bcm2835.h>

template <typename Rep, typename Period>
void sleepFor(std::chrono::duration<Rep, Period> durBase) {
    using namespace std::chrono_literals;
    auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(durBase);
    auto secs = dur / 1s;
    auto nsecs = (dur - (secs * 1s)) / 1ns;
    const struct timespec ts{secs, nsecs};
    nanosleep(&ts, NULL);
}

struct Color {
    uint8_t b = 0;
    uint8_t r = 0;
    uint8_t g = 0;
};

constexpr size_t outputBitsPerBit = 4;

constexpr uint32_t bswap(uint32_t val) {
    return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) | ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}

constexpr uint32_t byteConvert(uint8_t val) {
    uint32_t result = 0;
    for (int k = 0; k < 8; k++) {
        bool set = val & (1ull << k);
        result |= (set ? 0b1110 : 0b1000) << (k * outputBitsPerBit);
    }
    return bswap(result);
}

template <size_t... Inds>
constexpr auto precomputeByteConversions(std::integer_sequence<size_t, Inds...>) {
    return std::array<uint32_t, sizeof...(Inds)>{byteConvert(Inds)...};
}

constexpr auto precomputeByteConversions() {
    return precomputeByteConversions(std::make_index_sequence<256>{});
}

void computeFrame(std::chrono::high_resolution_clock::duration elapsed, Color* colors, size_t ledCount) {
    using namespace std::chrono_literals;
    auto elapsedMs = elapsed / 1ms;
    
    // Create an interesting mixture of colors
    for (size_t i = 0; i < ledCount; i++) {
        float indexAngle = 6 * M_PI / 180;
        float msAngle = 0.001 * 2 * M_PI;
        colors[i].r = (sin(i * indexAngle + elapsedMs * msAngle) + 1) * 127;
        colors[i].g = (sin(i * indexAngle + elapsedMs * -msAngle + 0.5) + 1) * 127;
        colors[i].b = (cos(i * indexAngle + elapsedMs * msAngle + 0.3) + 1) * 127;
    }
}

int main() {
    using namespace std::chrono_literals;
    constexpr std::array<uint32_t, 256> byteConversions = precomputeByteConversions();
    
    if (!bcm2835_init()) {
        return 1;
    }
    if (!bcm2835_spi_begin()) {
        return 1;
    }
    std::cout << "Starting" << std::endl;
    
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_set_speed_hz(3333333 / 2);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    
    unsigned fps = 24;
    
    constexpr size_t ledCount = 60;
    Color colors[ledCount];
    
    constexpr size_t bufferSize = ledCount * sizeof(Color) * outputBitsPerBit;
    constexpr size_t zerosSize = 50 * outputBitsPerBit;
    constexpr size_t transferSize = bufferSize + zerosSize;
    static_assert(sizeof(uint32_t) == outputBitsPerBit);
    uint32_t buffer[transferSize / sizeof(uint32_t)] = {0};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    while (true) {
        auto curTime = std::chrono::high_resolution_clock::now();
        computeFrame(curTime - startTime, colors, ledCount);
        
        uint8_t* src = reinterpret_cast<uint8_t*>(colors);
        uint8_t* end = reinterpret_cast<uint8_t*>(colors + ledCount);
        uint32_t* dst = buffer;
        for (; src != end; src++, dst++) {
            *dst = byteConversions[*src];
        }
        bcm2835_spi_writenb(reinterpret_cast<const char*>(buffer), transferSize);
        
        sleepFor(std::chrono::duration_cast<std::chrono::nanoseconds>(1s) / fps);
    }
    
    bcm2835_spi_end();
    bcm2835_close();

    return 0;
}
