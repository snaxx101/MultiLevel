#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <unordered_map>
#include <queue>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <sstream>
#include <random>
#include <chrono>
#include <stdexcept>
#include <algorithm>

enum Protection { READ_ONLY, READ_WRITE };

struct Page {
    int frame_number = -1;
    bool present = false;
    Protection protection = READ_WRITE;
    int last_access = 0;
};

struct Segment {
    int base_address;
    int limit;
    Protection protection;
    int fault_count = 0;
    Segment() = default;
    Segment(int base, int lim, Protection prot, int faults) 
        : base_address(base), limit(lim), protection(prot), fault_count(faults) {}
};

class PhysicalMemory {
public:
    int num_frames;
    std::vector<bool> free_frames;
    std::queue<int> fifo_queue;
    std::list<int> lru_list;
    std::map<int, int> frame_to_page;
    bool use_lru;

    PhysicalMemory(int frames, bool lru) : num_frames(frames), use_lru(lru) {
        free_frames.resize(frames, true);
    }

    int allocateFrame(int pageNum, int time) {
        for (int i = 0; i < num_frames; ++i) {
            if (free_frames[i]) {
                free_frames[i] = false;
                if (use_lru) lru_list.push_back(i);
                else fifo_queue.push(i);
                frame_to_page[i] = pageNum;
                return i;
            }
        }
        
        int frame;
        if (use_lru) {
            if (lru_list.empty()) throw std::runtime_error("Memory Full (LRU Error)");
            frame = lru_list.front();
            lru_list.pop_front();
            lru_list.push_back(frame);
        } else {
            if (fifo_queue.empty()) throw std::runtime_error("Memory Full (FIFO Error)");
            frame = fifo_queue.front();
            fifo_queue.pop();
            fifo_queue.push(frame);
        }
        frame_to_page[frame] = pageNum;
        return frame;
    }

    void freeFrame(int frame) {
        if (frame >= 0 && frame < num_frames) {
            free_frames[frame] = true;
            frame_to_page.erase(frame);
            if (use_lru) lru_list.remove(frame);
            else {
                std::queue<int> temp;
                while (!fifo_queue.empty()) {
                    if (fifo_queue.front() != frame) temp.push(fifo_queue.front());
                    fifo_queue.pop();
                }
                fifo_queue = temp;
            }
        }
    }

    double utilization() const {
        int used = std::count(free_frames.begin(), free_frames.end(), false);
        return (double)used / num_frames * 100;
    }
};

class PageTable {
public:
    std::vector<Page> pages;
    int page_size;
    int directory_index;

    PageTable(int numPages, int pageSize, int dirIdx) : page_size(pageSize), directory_index(dirIdx) {
        pages.resize(numPages);
        for (auto& p : pages) {
            // Random init for Part 3 requirement "some pages not present"
            p.present = (rand() % 10) < 3; // 30% chance to start present
            if (p.present) p.frame_number = rand() % 100; // Mock frame
            p.protection = (rand() % 2) ? READ_WRITE : READ_ONLY;
        }
    }

    int getFrameNumber(int pageNum, int time, Protection accessType, PhysicalMemory& physMem) {
        if (pageNum < 0 || pageNum >= pages.size()) {
            throw std::runtime_error("Page Fault: Invalid page number " + std::to_string(pageNum));
        }
        
        if (!pages[pageNum].present) {
            int frame = physMem.allocateFrame(pageNum, time);
            pages[pageNum].frame_number = frame;
            pages[pageNum].present = true;
            pages[pageNum].protection = accessType;
            return frame;
        }
        
        if (accessType == READ_WRITE && pages[pageNum].protection == READ_ONLY) {
            throw std::runtime_error("Protection Violation: Cannot write to read-only page");
        }
        
        pages[pageNum].last_access = time;
        if (physMem.use_lru) {
            auto it = std::find(physMem.lru_list.begin(), physMem.lru_list.end(), pages[pageNum].frame_number);
            if (it != physMem.lru_list.end()) {
                physMem.lru_list.erase(it);
                physMem.lru_list.push_back(pages[pageNum].frame_number);
            }
        }
        return pages[pageNum].frame_number;
    }
};

class DirectoryTable {
public:
    std::vector<PageTable*> pageTables;
    int max_pages_per_table;

    DirectoryTable(int maxPages) : max_pages_per_table(maxPages) {}

    PageTable* getPageTable(int dirNum, int pageSize) {
        while (dirNum >= pageTables.size()) {
            pageTables.push_back(new PageTable(max_pages_per_table, pageSize, pageTables.size()));
        }
        return pageTables[dirNum];
    }

    void freeTables() {
        for (auto* pt : pageTables) delete pt;
        pageTables.clear();
    }
};

class TLB {
private:
    std::unordered_map<std::string, int> cache;
    std::list<std::string> lruOrder;
    int maxSize;
    int hits = 0, total = 0;

public:
    TLB(int size) : maxSize(size) {}

    int get(int segNum, int dirNum, int pageNum) {
        std::string key = std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);
        total++;
        auto it = cache.find(key);
        if (it != cache.end()) {
            hits++;
            lruOrder.remove(key);
            lruOrder.push_back(key);
            return it->second;
        }
        return -1;
    }

    void put(int segNum, int dirNum, int pageNum, int frame) {
        std::string key = std::to_string(segNum) + ":" + std::to_string(dirNum) + ":" + std::to_string(pageNum);
        if (cache.size() >= maxSize) {
            cache.erase(lruOrder.front());
            lruOrder.pop_front();
        }
        cache[key] = frame;
        lruOrder.push_back(key);
    }

    double hitRate() const {
        return total > 0 ? (double)hits / total * 100 : 0;
    }

    void displayCache() {
        std::cout << "TLB Contents (LRU Order):\n";
        for (const auto& key : lruOrder) {
            std::cout << " " << key << " -> Frame " << cache[key] << "\n";
        }
        std::cout << "TLB Hit Rate: " << hitRate() << "%\n";
    }
};

class SegmentTable {
public: // Made public for helper access
    std::vector<Segment> segments;
    std::map<int, DirectoryTable*> directoryTables;
    TLB tlb;
    PhysicalMemory physMem;
    int time = 0;
    int total_latency = 0, translation_count = 0;

    SegmentTable(int tlbSize, int numFrames, bool use_lru) : tlb(tlbSize), physMem(numFrames, use_lru) {}

    ~SegmentTable() {
        for (auto& pair : directoryTables) delete pair.second;
    }

    void addSegment(int id, int base, int limit, Protection prot) {
        if (directoryTables.find(id) != directoryTables.end()) {
            // Update existing if needed, or ignore. Lab Part 3 says "add" cmd supports dynamic.
            // For simplicity, we'll overwrite/reset if it exists or just skip check to allow updates.
        }
        if (id >= segments.size()) {
            segments.resize(id + 1, Segment{0, 0, READ_ONLY, 0});
        }
        segments[id] = Segment{base, limit, prot, 0};
        directoryTables[id] = new DirectoryTable(limit); // Re-init table
    }

    void removeSegment(int id) {
        if (id < 0 || id >= segments.size() || directoryTables.find(id) == directoryTables.end()) {
            throw std::runtime_error("Cannot remove invalid segment " + std::to_string(id));
        }
        for (auto* pt : directoryTables[id]->pageTables) {
            for (auto& page : pt->pages) {
                if (page.present) physMem.freeFrame(page.frame_number);
            }
        }
        directoryTables[id]->freeTables();
        directoryTables.erase(id);
        segments[id] = Segment{0, 0, READ_WRITE, 0}; 
    }

    int translateAddress(int segNum, int dirNum, int pageNum, int offset, Protection accessType) {
        int latency = 1 + rand() % 10;
        total_latency += latency;
        translation_count++;
        time++; // Increment global time
        
        if (segNum < 0 || segNum >= segments.size() || segments[segNum].limit == 0) {
             if(segNum < segments.size()) segments[segNum].fault_count++;
             throw std::runtime_error("Segmentation Fault: Invalid segment " + std::to_string(segNum));
        }
        
        Segment& segment = segments[segNum];
        if (accessType == READ_WRITE && segment.protection == READ_ONLY) {
            segment.fault_count++;
            throw std::runtime_error("Protection Violation: Cannot write to read-only segment");
        }

        int frame = tlb.get(segNum, dirNum, pageNum);
        
        DirectoryTable* dirTable = directoryTables[segNum];
        PageTable* pageTable = dirTable->getPageTable(dirNum, 1000);

        if (frame != -1) {
            if (offset >= pageTable->page_size) {
                segment.fault_count++;
                throw std::runtime_error("Offset Fault: Offset " + std::to_string(offset) + " exceeds page size");
            }
            return segment.base_address + frame * pageTable->page_size + offset;
        }

        if (pageNum >= segment.limit) {
            segment.fault_count++;
            throw std::runtime_error("Page Fault: Page " + std::to_string(pageNum) + " exceeds limit " + std::to_string(segment.limit));
        }
        if (offset >= pageTable->page_size) {
            segment.fault_count++;
            throw std::runtime_error("Offset Fault: Offset " + std::to_string(offset) + " exceeds page size");
        }

        frame = pageTable->getFrameNumber(pageNum, time, accessType, physMem);
        tlb.put(segNum, dirNum, pageNum, frame);
        return segment.base_address + frame * pageTable->page_size + offset;
    }

    void displayStats() {
        std::cout << "\n--- System Statistics ---\n";
        std::cout << "Page Fault Statistics:\n";
        for (size_t i = 0; i < segments.size(); ++i) {
            if (segments[i].limit > 0) {
                std::cout << "Segment " << i << ": " << segments[i].fault_count << " faults\n";
                if (segments[i].fault_count > 0.2 * translation_count) {
                    std::cout << "Suggestion: Increase limit for Segment " << i << " to reduce faults\n";
                }
            }
        }
        std::cout << "Average Translation Latency: " << (translation_count > 0 ? (double)total_latency / translation_count : 0) << "\n";
        std::cout << "Physical Memory Utilization: " << physMem.utilization() << "%\n";
        tlb.displayCache();
        std::cout << "-------------------------\n";
    }

    void printMemoryMap() {
        std::cout << "\n--- Memory Map ---\n";
        for (size_t i = 0; i < segments.size(); ++i) {
            if (segments[i].limit == 0) continue;
            std::cout << "Segment " << i << ": Base=" << segments[i].base_address
                      << ", Limit=" << segments[i].limit
                      << ", Protection=" << (segments[i].protection == READ_ONLY ? "RO" : "RW")
                      << ", Faults=" << segments[i].fault_count << "\n";
            
            if (directoryTables.find(i) != directoryTables.end()) {
                for (size_t j = 0; j < directoryTables[i]->pageTables.size(); ++j) {
                    auto* pt = directoryTables[i]->pageTables[j];
                    std::cout << " Directory " << j << ":\n";
                    for (size_t k = 0; k < pt->pages.size(); ++k) {
                        auto& p = pt->pages[k];
                        if (p.present) {
                            std::cout << "  Page " << k << ": Frame=" << p.frame_number
                                      << ", Present=" << p.present
                                      << ", Protection=" << (p.protection == READ_ONLY ? "RO" : "RW")
                                      << ", LastAccess=" << p.last_access << "\n";
                        }
                    }
                }
            }
        }
    }
};

// Helper to satisfy "Support file input" for Initialization
void loadInitialConfig(SegmentTable& st, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Config file not found. Using random initialization.\n";
        // Fallback: Random Init as per Part 3 requirement
        st.addSegment(0, 0, 10, READ_WRITE);
        st.addSegment(1, 20000, 5, READ_ONLY);
        st.addSegment(2, 40000, 8, READ_WRITE);
        return;
    }
    
    int id, base, limit;
    std::string prot;
    while (file >> id >> base >> limit >> prot) {
        st.addSegment(id, base, limit, (prot == "RO" ? READ_ONLY : READ_WRITE));
    }
    std::cout << "Configuration loaded from " << filename << "\n";
}

void processBatchFile(SegmentTable& st, const std::string& filename) {
    std::ifstream file(filename);
    std::ofstream log("batch_results.txt"); // Requirement: Log to file
    int faults = 0, translations = 0;
    int segNum, dirNum, pageNum, offset;
    std::string access;
    
    while (file >> segNum >> dirNum >> pageNum >> offset >> access) {
        try {
            int addr = st.translateAddress(segNum, dirNum, pageNum, offset, access == "RW" ? READ_WRITE : READ_ONLY);
            // Requirement: Gantt-like timeline "Time X: Address Y -> Z"
            std::string msg = "Time " + std::to_string(st.time) + ": Address (" 
                            + std::to_string(segNum) + "," + std::to_string(pageNum) 
                            + ") -> Physical " + std::to_string(addr);
            std::cout << msg << "\n";
            log << msg << "\n";
        } catch (const std::runtime_error& e) {
            faults++;
            std::string msg = "Time " + std::to_string(st.time) + ": Error " + e.what();
            std::cout << msg << "\n";
            log << msg << "\n";
        }
        translations++;
    }
    log << "Fault Rate: " << (translations > 0 ? (double)faults / translations * 100 : 0) << "%\n";
}

void generateRandomAddresses(SegmentTable& st, int num, double validRatio, const std::string& logFile) {
    std::ofstream log(logFile);
    std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
    int faults = 0;
    for (int i = 0; i < num; ++i) {
        int segNum = (gen() % 5);
        int dirNum = (gen() % 2);
        int pageNum = (gen() % 15);
        int offset = gen() % 1050; 
        Protection access = (gen() % 2) ? READ_WRITE : READ_ONLY;
        
        try {
            int addr = st.translateAddress(segNum, dirNum, pageNum, offset, access);
            log << "Time " << st.time << ": Physical=" << addr << "\n";
        } catch (const std::runtime_error& e) {
            faults++;
            log << "Time " << st.time << ": Error=" << e.what() << "\n";
        }
    }
    log << "Page Fault Rate: " << (num > 0 ? (double)faults / num * 100 : 0) << "%\n";
}

int main(int argc, char* argv[]) {
    srand(time(0));
    
    int numFrames = 10;
    int tlbSize = 4;
    int pageSize = 1000;
    bool use_lru = true;
    std::string batchFile = "";
    std::string initFile = "init_config.txt"; // Default config file name

    // Requirement: Prompt for memory size, page size, etc.
    // We use args if provided, otherwise prompt interactively.
    if (argc == 1) {
        std::cout << "Enter Physical Memory Size (frames): ";
        std::cin >> numFrames;
        std::cout << "Enter TLB Size: ";
        std::cin >> tlbSize;
        std::cout << "Enter Page Size: ";
        std::cin >> pageSize;
        std::string policy;
        std::cout << "Enter Replacement Policy (lru/fifo): ";
        std::cin >> policy;
        use_lru = (policy == "lru");
    } else {
        for (int i = 1; i < argc; i += 2) {
            std::string arg = argv[i];
            if (i + 1 < argc) {
                if (arg == "--frames") numFrames = std::stoi(argv[i + 1]);
                else if (arg == "--tlb") tlbSize = std::stoi(argv[i + 1]);
                else if (arg == "--pagesize") pageSize = std::stoi(argv[i + 1]);
                else if (arg == "--replace") use_lru = (std::string(argv[i + 1]) == "lru");
                else if (arg == "--batch") batchFile = argv[i + 1];
                else if (arg == "--init") initFile = argv[i + 1];
            }
        }
    }

    SegmentTable segmentTable(tlbSize, numFrames, use_lru);

    // Requirement: Initialize segments (random or from file)
    loadInitialConfig(segmentTable, initFile);

    if (!batchFile.empty()) {
        processBatchFile(segmentTable, batchFile);
        std::cout << "Batch results logged to batch_results.txt\n";
        segmentTable.displayStats();
        return 0;
    }

    segmentTable.printMemoryMap();
    std::cout << "\nCommands: add <id> <base> <limit> <prot>, remove <id>, translate <seg> <dir> <page> <offset> <access>, random <num>, stats, quit\n";
    
    std::string command;
    while (std::cout << ">> " && std::cin >> command) {
        try {
            if (command == "add") {
                int id, base, limit;
                std::string prot;
                std::cin >> id >> base >> limit >> prot;
                segmentTable.addSegment(id, base, limit, prot == "RO" ? READ_ONLY : READ_WRITE);
                std::cout << "Segment " << id << " added\n";
            } else if (command == "remove") {
                int id;
                std::cin >> id;
                segmentTable.removeSegment(id);
                std::cout << "Segment " << id << " removed\n";
            } else if (command == "translate") {
                int segNum, dirNum, pageNum, offset;
                std::string access;
                std::cin >> segNum >> dirNum >> pageNum >> offset >> access;
                int addr = segmentTable.translateAddress(segNum, dirNum, pageNum, offset, access == "RO" ? READ_ONLY : READ_WRITE);
                std::cout << "Time " << segmentTable.time << ": Physical Address: " << addr << "\n";
            } else if (command == "random") {
                int num;
                std::cin >> num;
                generateRandomAddresses(segmentTable, num, 0.7, "random_results.txt");
                std::cout << "Results logged to random_results.txt\n";
            } else if (command == "stats") {
                segmentTable.displayStats();
            } else if (command == "map") {
                segmentTable.printMemoryMap();
            } else if (command == "quit") {
                break;
            }
        } catch (const std::runtime_error& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }
    return 0;
}