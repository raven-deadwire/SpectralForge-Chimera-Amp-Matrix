#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <filesystem>

namespace spectralforge::cabIRFile {
inline juce::Result writeBytes(const juce::File& file,const juce::MemoryBlock& bytes) {
    auto stream=file.createOutputStream();
    if(!stream || stream->getStatus().failed())return juce::Result::fail("Cannot write "+file.getFullPathName());
    if(!stream->write(bytes.getData(),bytes.getSize()))return juce::Result::fail("Cannot write the IR samples");
    stream->flush(); // buffered failures must be observed before committing
    return stream->getStatus();
}
// The injected writer lets the regression test fail after a partial write or
// flush, while exercising the exact production transaction/cleanup path.
template <typename Writer>
juce::Result writeAtomically(const juce::File& file,const juce::MemoryBlock& bytes,Writer&& write) {
    if(file.isDirectory())return juce::Result::fail("The destination is a directory");
    if(!file.getParentDirectory().createDirectory())return juce::Result::fail("Cannot create "+file.getParentDirectory().getFullPathName());
    juce::TemporaryFile temporary(file);
    const auto written=write(temporary.getFile(),bytes);
    if(written.failed())return written;
    // Same-directory rename, with no copy fallback that could truncate the
    // old file on failure. Paths retain Unicode on Windows.
    std::error_code error;
    std::filesystem::rename(std::filesystem::u8path(temporary.getFile().getFullPathName().toStdString()),
                            std::filesystem::u8path(file.getFullPathName().toStdString()),error);
    return error?juce::Result::fail("Cannot replace "+file.getFullPathName()+": "+juce::String(error.message())):juce::Result::ok();
}
inline juce::Result writeAtomically(const juce::File& file,const juce::MemoryBlock& bytes) {return writeAtomically(file,bytes,writeBytes);}
inline juce::File wavDestination(const juce::File& selected) {return selected.withFileExtension(".wav");}
inline bool needsAdditionalOverwriteConfirmation(const juce::File& selected) {
    const auto target=wavDestination(selected);return target!=selected && target.exists();
}
}
