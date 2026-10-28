#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace spectralforge {

// The children of a PARAMS tree by their "id", like
// ValueTree::getChildWithProperty("id", ...) (first match wins) without its
// linear scan. With about 5000 parameters, a recall that looked up every one
// that way took close to a second on the message thread.
class ParamIndex {
public:
    explicit ParamIndex(const juce::ValueTree& tree) {for(const auto& child:tree)add(child);}
    juce::ValueTree operator[](const juce::String& id) const {return map.contains(id)?map[id]:juce::ValueTree{};}
    void add(const juce::ValueTree& child) {
        const auto id=child.getProperty("id").toString();
        if(id.isNotEmpty() && !map.contains(id))map.set(id,child);
    }
private:
    juce::HashMap<juce::String,juce::ValueTree> map{8191};
};

}
