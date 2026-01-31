/*
 * OpenClaw C++11 - Memory Manager
 * 
 * High-level interface for memory operations.
 * Handles file discovery, chunking, indexing, and search.
 */
#ifndef OPENCLAW_MEMORY_MANAGER_HPP
#define OPENCLAW_MEMORY_MANAGER_HPP

#include "types.hpp"
#include "store.hpp"
#include <string>
#include <vector>
#include <memory>

namespace openclaw {

class MemoryManager {
public:
    explicit MemoryManager(const MemoryConfig& config);
    ~MemoryManager();
    
    // Initialize the memory system
    bool initialize();
    void shutdown();
    bool is_initialized() const;
    
    // Sync operations - scan and index memory files
    bool sync();
    
    // Memory file operations
    bool save_memory(const std::string& content, const std::string& filename = "");
    bool save_daily_memory(const std::string& content);  // memory/YYYY-MM-DD.md
    std::string load_memory_file(const std::string& path);
    bool append_to_memory(const std::string& content, const std::string& filename = "MEMORY.md");
    std::vector<std::string> list_memory_files();
    
    // Search operations
    std::vector<MemorySearchResult> search(const std::string& query);
    std::vector<MemorySearchResult> search(const std::string& query, const MemorySearchConfig& config);
    
    // Get file content by path
    std::string get_memory_content(const std::string& path);
    
    // Task operations
    std::string create_task(const std::string& content, const std::string& context = "");
    bool complete_task(const std::string& task_id);
    std::vector<MemoryTask> list_tasks(bool include_completed = false);
    std::vector<MemoryTask> get_pending_tasks();
    std::vector<MemoryTask> get_tasks_due_soon(int hours = 24);
    bool update_task_due(const std::string& task_id, int64_t due_at);
    
    // Utility
    std::string last_error() const;
    const MemoryConfig& config() const;
    
private:
    MemoryConfig config_;
    std::unique_ptr<MemoryStore> store_;
    bool initialized_;
    std::string last_error_;
    
    // File discovery
    std::vector<MemoryFile> discover_memory_files();
    MemoryFile build_file_entry(const std::string& abs_path);
    
    // Indexing
    bool index_file(const MemoryFile& file);
    std::vector<MemoryChunk> chunk_content(const std::string& content, 
                                           const std::string& path,
                                           MemorySource source);
    
    // Chunking helpers
    std::vector<std::string> split_into_paragraphs(const std::string& content);
    std::string compute_hash(const std::string& content);
    std::string generate_uuid();
    
    // Date helpers
    std::string get_today_date();
    int64_t get_current_timestamp();
    
    void set_error(const std::string& error);
};

} // namespace openclaw

#endif // OPENCLAW_MEMORY_MANAGER_HPP
