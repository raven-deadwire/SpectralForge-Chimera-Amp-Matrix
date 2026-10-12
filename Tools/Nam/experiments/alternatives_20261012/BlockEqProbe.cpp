// Isolated experiment: render with the pinned, unmodified TONE3000 BlockEq.
// No NAM weights, arbitrary coefficient injection, or shipping DSP changes.
#include "BlockEq.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void configure(BlockEq& eq, const juce::var& job) {
  const auto e = job["eq"];
  const auto* bands = e["bands"].getArray();
  if (!e.isObject() || bands == nullptr || bands->size() != 6)
    throw std::runtime_error("Exactly six EQ bands are required");
  for (int i = 0; i < 6; ++i)
    if (!eq.setBandFromVar(i, (*bands)[i]))
      throw std::runtime_error("Invalid EQ band");
  eq.setEnabled(bool(e["enabled"]));
  eq.setPre(bool(e["pre"]));
  eq.prepare(48000.0);
}

void process(BlockEq& eq, std::vector<float>& x, int block) {
  for (size_t start = 0; start < x.size(); start += size_t(block)) {
    const int count = int(std::min(size_t(block), x.size() - start));
    float* p = x.data() + start;
    juce::AudioBuffer<float> buffer(&p, 1, count);
    if (eq.isActive()) eq.process(buffer);
  }
}
}  // namespace

int main(int argc, char** argv) try {
  if (argc != 4)
    throw std::runtime_error("Usage: BlockEqProbe input.f32 output.f32 job.json");
  const auto job = juce::JSON::parse(juce::File(argv[3]));
  if (!job.isObject()) throw std::runtime_error("Invalid job JSON");
  const int block = int(job.getProperty("block_size", 64));
  if (block < 1 || block > 4096) throw std::runtime_error("Invalid block size");
  if (int(job.getProperty("sample_rate", 48000)) != 48000)
    throw std::runtime_error("The pinned target's base rate is 48000 Hz");
  if (bool(job.getProperty("normalize", false)))
    throw std::runtime_error("EQ-only probe does not apply model normalization");

  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Cannot read input");
  const auto bytes = input.tellg();
  if (bytes <= 0 || bytes % 4) throw std::runtime_error("Invalid float32 input length");
  std::vector<float> output(size_t(bytes) / 4);
  input.seekg(0);
  input.read(reinterpret_cast<char*>(output.data()), bytes);
  if (!input) throw std::runtime_error("Incomplete input read");
  for (const float value : output)
    if (!std::isfinite(value)) throw std::runtime_error("Nonfinite input");
  std::vector<float> restoredOutput = output;

  BlockEq original;
  configure(original, job);
  const auto savedTree = original.toValueTree();
  juce::MemoryOutputStream stream;
  savedTree.writeToStream(stream);
  const auto readTree = juce::ValueTree::readFromData(stream.getData(), stream.getDataSize());
  if (!savedTree.isEquivalentTo(readTree))
    throw std::runtime_error("Serialized native EQ tree differs after restore");
  BlockEq restored;
  restored.restoreFromValueTree(readTree);
  restored.prepare(48000.0);
  const auto originalParameters = juce::JSON::toString(original.toVar(), true);
  const auto restoredParameters = juce::JSON::toString(restored.toVar(), true);
  if (originalParameters != restoredParameters)
    throw std::runtime_error("Restored native EQ parameters differ");

  process(original, output, block);
  process(restored, restoredOutput, block);
  double power = 0.0, peak = 0.0, maxDelta = 0.0;
  for (size_t i = 0; i < output.size(); ++i) {
    if (!std::isfinite(output[i]) || !std::isfinite(restoredOutput[i]))
      throw std::runtime_error("Nonfinite EQ output");
    power += double(restoredOutput[i]) * restoredOutput[i];
    peak = std::max(peak, std::abs(double(restoredOutput[i])));
    maxDelta = std::max(maxDelta, std::abs(double(restoredOutput[i]) - output[i]));
  }
  const bool bitExact = std::memcmp(output.data(), restoredOutput.data(), size_t(bytes)) == 0;
  if (!bitExact) throw std::runtime_error("Native EQ output changed after serialization");
  std::ofstream destination(argv[2], std::ios::binary | std::ios::trunc);
  destination.write(reinterpret_cast<const char*>(restoredOutput.data()), bytes);
  if (!destination) throw std::runtime_error("Output write failed");

  auto* obj = new juce::DynamicObject();
  obj->setProperty("sample_rate", 48000);
  obj->setProperty("block_size", block);
  obj->setProperty("samples", juce::int64(output.size()));
  obj->setProperty("peak", peak);
  obj->setProperty("rms", std::sqrt(power / output.size()));
  obj->setProperty("eq", restored.toVar());
  obj->setProperty("native_eq_tree_roundtrip", true);
  obj->setProperty("roundtrip_output_bit_exact", bitExact);
  obj->setProperty("roundtrip_max_abs_delta", maxDelta);
  obj->setProperty("serialization_scope", "BlockEq native ValueTree; not full plugin/DAW acceptance");
  std::cout << juce::JSON::toString(juce::var(obj), true).toStdString() << '\n';
  return 0;
} catch (const std::exception& error) {
  std::cerr << error.what() << '\n';
  return 1;
}
