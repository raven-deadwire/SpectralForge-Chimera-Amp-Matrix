#include <juce_core/juce_core.h>
#include <iostream>
#include <iomanip>
int main() {
  for (float start : {-24.f, -36.f, 20.f}) {
    juce::NormalisableRange<float> range(start, start == 20.f ? 20000.f : start+48.f, .01f);
    if (start == 20.f) range.setSkewForCentre(1000.f);
    volatile float requested = start == -24.f ? 0.f : start == -36.f ? -9.f : 1000.f;
    auto normal = range.convertTo0to1(range.snapToLegalValue(requested));
    auto actual = range.snapToLegalValue(range.convertFrom0to1(normal));
    std::cout << std::setprecision(9) << "requested=" << requested << " stored=" << actual << " equal=" << (requested==actual) << '\n';
  }
}
