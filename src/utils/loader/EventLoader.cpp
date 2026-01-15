#include "EventLoader.hpp"

namespace EventLoader {

std::vector<std::shared_ptr<Event>> EventParser::parseEventFile(const std::string& filename) {
    std::vector<std::shared_ptr<Event>> events;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open event file: " + filename);
    }
    
    std::string line;
    int lineNumber = 0;
    
    while (std::getline(file, line)) {
        ++lineNumber;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        try {
            auto event = parseLine(line);
            if (event) {
                events.push_back(event);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Error parsing event file at line " + 
                                    std::to_string(lineNumber) + ": " + e.what());
        }
    }
    
    file.close();
    
    // Verify that timestamps are nondecreasing
    for (size_t i = 1; i < events.size(); ++i) {
        if (events[i]->timestamp < events[i-1]->timestamp) {
            throw std::runtime_error("Events are not in chronological order");
        }
    }
    
    return events;
}

std::shared_ptr<Event> EventParser::parseLine(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    
    // Parse timestamp
    if (!std::getline(iss, token, ' ')) {
        throw std::runtime_error("Missing timestamp");
    }
    
    uint64_t timestamp;
    try {
        timestamp = std::stoull(token);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid timestamp: " + token);
    }
    
    // Parse event type
    if (!std::getline(iss, token, ' ')) {
        throw std::runtime_error("Missing event type");
    }
    
    std::string eventType = token;
    
    // Parse event based on type
    if (eventType == "AVAILABLE") {
        return std::make_shared<AvailableEvent>(timestamp);
    }
    else if (eventType == "PLAY") {
        double playbackTime, playbackRate;
        
        if (!std::getline(iss, token, ' ')) {
            throw std::runtime_error("PLAY: missing playback time");
        }
        try {
            playbackTime = std::stod(token);
        } catch (const std::exception&) {
            throw std::runtime_error("PLAY: invalid playback time: " + token);
        }
        
        if (!std::getline(iss, token, ' ')) {
            throw std::runtime_error("PLAY: missing playback rate");
        }
        try {
            playbackRate = std::stod(token);
        } catch (const std::exception&) {
            throw std::runtime_error("PLAY: invalid playback rate: " + token);
        }
        
        return std::make_shared<PlayEvent>(timestamp, playbackTime, playbackRate);
    }
    else if (eventType == "SAVE") {
        if (!std::getline(iss, token, ' ')) {
            throw std::runtime_error("SAVE: missing filename");
        }
        
        std::string filename = token;
        if (filename.find(".ppm") == std::string::npos) {
            std::cerr << "Warning: SAVE filename does not end with .ppm: " << filename << std::endl;
        }
        
        return std::make_shared<SaveEvent>(timestamp, filename);
    }
    else if (eventType == "MARK") {
        // Get remaining part of the line as description
        std::string description;
        std::getline(iss, description);
        
        // Trim leading whitespace
        size_t start = description.find_first_not_of(" \t");
        if (start != std::string::npos) {
            description = description.substr(start);
        }
        
        return std::make_shared<MarkEvent>(timestamp, description);
    }
    else {
        throw std::runtime_error("Unknown event type: " + eventType);
    }
}

void EventParser::processEvents(
    const std::vector<std::shared_ptr<Event>>& events,
    std::function<void(const std::shared_ptr<AvailableEvent>&)> onAvailable,
    std::function<void(const std::shared_ptr<PlayEvent>&)> onPlay,
    std::function<void(const std::shared_ptr<SaveEvent>&)> onSave,
    std::function<void(const std::shared_ptr<MarkEvent>&)> onMark
) {
    for (const auto& event : events) {
        if (auto availableEvent = std::dynamic_pointer_cast<AvailableEvent>(event)) {
            if (onAvailable) onAvailable(availableEvent);
        }
        else if (auto playEvent = std::dynamic_pointer_cast<PlayEvent>(event)) {
            if (onPlay) onPlay(playEvent);
        }
        else if (auto saveEvent = std::dynamic_pointer_cast<SaveEvent>(event)) {
            if (onSave) onSave(saveEvent);
        }
        else if (auto markEvent = std::dynamic_pointer_cast<MarkEvent>(event)) {
            if (onMark) onMark(markEvent);
        }
    }
}

}  // namespace MyVK::Utils::Loader
