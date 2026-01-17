#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>

int get_volume() {
    FILE* pipe = popen("pamixer --get-volume 2>/dev/null", "r");
    if (!pipe) return -1;
    int volume = -1;
    if (fscanf(pipe, "%d", &volume) != 1)
        volume = -1;
    pclose(pipe);
    return volume;
}

void set_volume(int volume) {
    if (volume < 0) return;
    std::string cmd = "pamixer --set-volume " + std::to_string(volume);
    system(cmd.c_str());
}

bool pipewire_ready() {
    return get_volume() >= 0;
}

// Retry setting volume until it sticks
bool set_volume_with_retry(int target_volume, int max_attempts = 10) {
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        set_volume(target_volume);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        int current = get_volume();
        if (current == target_volume) {
            return true;
        }
        
        std::cout << "Volume mismatch (attempt " << (attempt + 1) 
                  << "): expected " << target_volume 
                  << "%, got " << current << "%\n";
    }
    return false;
}

int main() {
    while (true) {
        int saved_volume = get_volume();
        if (saved_volume < 0) {
            std::cerr << "Failed to read volume\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        std::cout << "Restarting PipeWire (saved volume "
                  << saved_volume << "%)\n";
        system("systemctl --user restart pipewire");
        
        // Wait for PipeWire to be ready
        for (int i = 0; i < 20; ++i) {
            if (pipewire_ready())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Give PipeWire extra time to finish initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Restore volume with retry logic
        if (set_volume_with_retry(saved_volume)) {
            std::cout << "Volume restored to " << saved_volume << "%\n";
        } else {
            std::cerr << "Failed to restore volume after multiple attempts\n";
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

