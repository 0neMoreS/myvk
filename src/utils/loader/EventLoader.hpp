#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <functional>

namespace EventLoader {

// Base class for all event types
class Event {
public:
    uint64_t timestamp;  // microseconds since first frame
    
    explicit Event(uint64_t ts) : timestamp(ts) {}
    virtual ~Event() = default;
    
    virtual std::string getType() const = 0;
};

// AVAILABLE event - make image available for rendering
class AvailableEvent : public Event {
public:
    explicit AvailableEvent(uint64_t ts) : Event(ts) {}
    
    std::string getType() const override {
        return "AVAILABLE";
    }
};

// PLAY event - set animation playback time and rate
class PlayEvent : public Event {
public:
    double playbackTime;  // time in seconds
    double playbackRate;  // playback rate (0 = paused, 1 = real-time)
    
    PlayEvent(uint64_t ts, double t, double rate)
        : Event(ts), playbackTime(t), playbackRate(rate) {}
    
    std::string getType() const override {
        return "PLAY";
    }
};

// SAVE event - save rendering to PPM file
class SaveEvent : public Event {
public:
    std::string filename;  // output filename (must end with .ppm)
    
    SaveEvent(uint64_t ts, const std::string& file)
        : Event(ts), filename(file) {}
    
    std::string getType() const override {
        return "SAVE";
    }
};

// MARK event - print mark to stdout
class MarkEvent : public Event {
public:
    std::string description;  // mark description and parameters
    
    MarkEvent(uint64_t ts, const std::string& desc)
        : Event(ts), description(desc) {}
    
    std::string getType() const override {
        return "MARK";
    }
};

// Event parser
class EventParser {
public:
    // Parse event file and return vector of events
    static std::vector<std::shared_ptr<Event>> parseEventFile(const std::string& filename);
    
    // Parse a single line from event file
    static std::shared_ptr<Event> parseLine(const std::string& line);
    
    // Helper function to process events
    static void processEvents(
        const std::vector<std::shared_ptr<Event>>& events,
        std::function<void(const std::shared_ptr<AvailableEvent>&)> onAvailable = nullptr,
        std::function<void(const std::shared_ptr<PlayEvent>&)> onPlay = nullptr,
        std::function<void(const std::shared_ptr<SaveEvent>&)> onSave = nullptr,
        std::function<void(const std::shared_ptr<MarkEvent>&)> onMark = nullptr
    );
};

}  // namespace MyVK::Utils::Loader
