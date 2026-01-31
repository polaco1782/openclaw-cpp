/*
 * OpenClaw C++11 - Memory Types
 * 
 * Data structures for the memory/recall system.
 * Based on OpenClaw's semantic memory architecture.
 */
#ifndef OPENCLAW_MEMORY_TYPES_HPP
#define OPENCLAW_MEMORY_TYPES_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace openclaw {

// Source of memory content
enum class MemorySource {
    MEMORY,     // From MEMORY.md or memory/*.md
    SESSIONS,   // From session transcripts
    TASK        // From task/reminder system
};

inline std::string memory_source_to_string(MemorySource s) {
    switch (s) {
        case MemorySource::MEMORY: return "memory";
        case MemorySource::SESSIONS: return "sessions";
        case MemorySource::TASK: return "task";
    }
    return "memory";
}

inline MemorySource string_to_memory_source(const std::string& s) {
    if (s == "sessions") return MemorySource::SESSIONS;
    if (s == "task") return MemorySource::TASK;
    return MemorySource::MEMORY;
}

// A file tracked in the memory index
struct MemoryFile {
    std::string path;       // Relative path from workspace
    std::string abs_path;   // Absolute path
    MemorySource source;    // Where it came from
    std::string hash;       // SHA256 of content
    int64_t mtime;          // Modification time (unix ms)
    int64_t size;           // File size in bytes
    
    MemoryFile() : source(MemorySource::MEMORY), mtime(0), size(0) {}
};

// A chunk of text from a memory file (for indexing/search)
struct MemoryChunk {
    std::string id;         // Unique chunk ID (uuid or hash-based)
    std::string path;       // File path this chunk is from
    MemorySource source;    // Source type
    int start_line;         // Starting line number (1-indexed)
    int end_line;           // Ending line number (1-indexed)
    std::string text;       // The actual text content
    std::string hash;       // Hash of text content
    int64_t updated_at;     // When last updated (unix ms)
    
    MemoryChunk() : source(MemorySource::MEMORY), start_line(0), end_line(0), updated_at(0) {}
};

// Result from a memory search
struct MemorySearchResult {
    std::string path;       // File path
    int start_line;         // Starting line of snippet
    int end_line;           // Ending line of snippet
    double score;           // Relevance score (0-1)
    std::string snippet;    // Text snippet
    MemorySource source;    // Source type
    
    MemorySearchResult() : start_line(0), end_line(0), score(0), source(MemorySource::MEMORY) {}
};

// A task or reminder stored in memory
struct MemoryTask {
    std::string id;         // Unique task ID
    std::string content;    // Task description
    std::string context;    // Additional context/notes
    std::string channel;    // Channel where task was created
    std::string user_id;    // User who created it
    int64_t created_at;     // Creation timestamp
    int64_t due_at;         // Due date (0 = no due date)
    bool completed;         // Whether task is done
    int64_t completed_at;   // When completed
    
    MemoryTask() : created_at(0), due_at(0), completed(false), completed_at(0) {}
};

// Configuration for memory chunking
struct ChunkingConfig {
    int target_tokens;      // Target tokens per chunk (~400)
    int overlap_tokens;     // Overlap between chunks (~80)
    int chars_per_token;    // Approximate chars per token (~4)
    
    ChunkingConfig() : target_tokens(400), overlap_tokens(80), chars_per_token(4) {}
    
    int max_chars() const { return target_tokens * chars_per_token; }
    int overlap_chars() const { return overlap_tokens * chars_per_token; }
};

// Configuration for memory search
struct MemorySearchConfig {
    int max_results;        // Maximum results to return
    double min_score;       // Minimum relevance score
    bool hybrid_enabled;    // Use hybrid BM25 + vector search
    double bm25_weight;     // Weight for BM25 keyword search
    double vector_weight;   // Weight for vector similarity
    
    MemorySearchConfig() 
        : max_results(10)
        , min_score(0.1)
        , hybrid_enabled(false)  // Vector search not implemented yet
        , bm25_weight(1.0)
        , vector_weight(0.0)
    {}
};

// Overall memory configuration
struct MemoryConfig {
    std::string workspace_dir;      // Agent workspace directory
    std::string db_path;            // SQLite database path
    ChunkingConfig chunking;
    MemorySearchConfig search;
    bool watch_enabled;             // Watch files for changes
    int sync_interval_minutes;      // Auto-sync interval (0 = disabled)
    
    MemoryConfig() : watch_enabled(false), sync_interval_minutes(0) {}
};

} // namespace openclaw

#endif // OPENCLAW_MEMORY_TYPES_HPP
