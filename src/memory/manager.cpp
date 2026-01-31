/*
 * OpenClaw C++11 - Memory Manager Implementation
 */
#include "../../include/openclaw/memory/manager.hpp"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <sys/stat.h>
#include <dirent.h>
#include <openssl/sha.h>

namespace openclaw {

MemoryManager::MemoryManager(const MemoryConfig& config)
    : config_(config)
    , store_(new MemoryStore())
    , initialized_(false)
{
}

MemoryManager::~MemoryManager() {
    shutdown();
}

bool MemoryManager::initialize() {
    if (initialized_) return true;
    
    // Ensure workspace directory exists
    struct stat st;
    if (stat(config_.workspace_dir.c_str(), &st) != 0) {
        set_error("Workspace directory does not exist: " + config_.workspace_dir);
        return false;
    }
    
    // Determine database path
    std::string db_path = config_.db_path;
    if (db_path.empty()) {
        db_path = config_.workspace_dir + "/.openclaw/memory.db";
    }
    
    // Ensure parent directory exists
    std::string db_dir = db_path.substr(0, db_path.rfind('/'));
    if (!db_dir.empty() && stat(db_dir.c_str(), &st) != 0) {
        // Create directory
        std::string cmd = "mkdir -p " + db_dir;
        system(cmd.c_str());
    }
    
    // Open database
    if (!store_->open(db_path)) {
        set_error("Failed to open database: " + store_->last_error());
        return false;
    }
    
    // Initialize schema
    if (!store_->ensure_schema()) {
        set_error("Failed to create schema: " + store_->last_error());
        return false;
    }
    
    initialized_ = true;
    return true;
}

void MemoryManager::shutdown() {
    if (store_) {
        store_->close();
    }
    initialized_ = false;
}

bool MemoryManager::is_initialized() const {
    return initialized_;
}

bool MemoryManager::sync() {
    if (!initialized_) {
        set_error("Memory manager not initialized");
        return false;
    }
    
    // Discover memory files
    std::vector<MemoryFile> files = discover_memory_files();
    std::vector<std::string> active_paths;
    
    for (const auto& file : files) {
        active_paths.push_back(file.path);
        
        // Check if file has changed
        MemoryFile existing;
        bool exists = store_->get_file(file.path, MemorySource::MEMORY, existing);
        
        if (exists && existing.hash == file.hash) {
            continue;  // File unchanged
        }
        
        // Index the file
        if (!index_file(file)) {
            // Log error but continue with other files
        }
    }
    
    // Remove stale files
    std::vector<std::string> stale = store_->get_stale_paths(active_paths, MemorySource::MEMORY);
    for (const auto& path : stale) {
        store_->delete_chunks_for_file(path, MemorySource::MEMORY);
        store_->delete_file(path, MemorySource::MEMORY);
    }
    
    return true;
}

bool MemoryManager::save_memory(const std::string& content, const std::string& filename) {
    std::string target = filename.empty() ? "MEMORY.md" : filename;
    std::string full_path = config_.workspace_dir + "/" + target;
    
    std::ofstream file(full_path.c_str());
    if (!file) {
        set_error("Failed to open file for writing: " + full_path);
        return false;
    }
    
    file << content;
    file.close();
    
    // Trigger sync to index the new content
    return sync();
}

bool MemoryManager::save_daily_memory(const std::string& content) {
    // Create memory directory if needed
    std::string memory_dir = config_.workspace_dir + "/memory";
    struct stat st;
    if (stat(memory_dir.c_str(), &st) != 0) {
        std::string cmd = "mkdir -p " + memory_dir;
        system(cmd.c_str());
    }
    
    // Get today's date
    std::string date = get_today_date();
    std::string filename = "memory/" + date + ".md";
    
    return save_memory(content, filename);
}

std::string MemoryManager::load_memory_file(const std::string& path) {
    std::string full_path = config_.workspace_dir + "/" + path;
    
    std::ifstream file(full_path.c_str());
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool MemoryManager::append_to_memory(const std::string& content, const std::string& filename) {
    std::string full_path = config_.workspace_dir + "/" + filename;
    
    std::ofstream file(full_path.c_str(), std::ios::app);
    if (!file) {
        set_error("Failed to open file for appending: " + full_path);
        return false;
    }
    
    file << "\n" << content;
    file.close();
    
    return sync();
}

std::vector<std::string> MemoryManager::list_memory_files() {
    std::vector<std::string> result;
    std::vector<MemoryFile> files = store_->list_files(MemorySource::MEMORY);
    for (const auto& f : files) {
        result.push_back(f.path);
    }
    return result;
}

std::vector<MemorySearchResult> MemoryManager::search(const std::string& query) {
    return search(query, config_.search);
}

std::vector<MemorySearchResult> MemoryManager::search(const std::string& query, 
                                                       const MemorySearchConfig& config) {
    if (!initialized_) {
        return std::vector<MemorySearchResult>();
    }
    return store_->search(query, config);
}

std::string MemoryManager::get_memory_content(const std::string& path) {
    return load_memory_file(path);
}

// Task operations
std::string MemoryManager::create_task(const std::string& content, const std::string& context) {
    if (!initialized_) {
        set_error("Memory manager not initialized");
        return "";
    }
    
    MemoryTask task;
    task.id = generate_uuid();
    task.content = content;
    task.context = context;
    task.created_at = get_current_timestamp();
    task.due_at = 0;
    task.completed = false;
    task.completed_at = 0;
    
    if (store_->upsert_task(task)) {
        return task.id;
    }
    return "";
}

bool MemoryManager::complete_task(const std::string& task_id) {
    if (!initialized_) return false;
    return store_->complete_task(task_id);
}

std::vector<MemoryTask> MemoryManager::list_tasks(bool include_completed) {
    if (!initialized_) return std::vector<MemoryTask>();
    return store_->list_tasks(include_completed);
}

std::vector<MemoryTask> MemoryManager::get_pending_tasks() {
    if (!initialized_) return std::vector<MemoryTask>();
    return store_->get_pending_tasks();
}

std::vector<MemoryTask> MemoryManager::get_tasks_due_soon(int hours) {
    if (!initialized_) return std::vector<MemoryTask>();
    int64_t now = get_current_timestamp();
    int64_t deadline = now + (static_cast<int64_t>(hours) * 3600 * 1000);
    return store_->get_tasks_due_before(deadline);
}

bool MemoryManager::update_task_due(const std::string& task_id, int64_t due_at) {
    if (!initialized_) return false;
    
    MemoryTask task;
    if (!store_->get_task(task_id, task)) {
        return false;
    }
    
    task.due_at = due_at;
    return store_->upsert_task(task);
}

std::string MemoryManager::last_error() const {
    return last_error_;
}

const MemoryConfig& MemoryManager::config() const {
    return config_;
}

// Private methods

std::vector<MemoryFile> MemoryManager::discover_memory_files() {
    std::vector<MemoryFile> result;
    
    // Check for MEMORY.md (case-insensitive)
    std::vector<std::string> main_files = {"MEMORY.md", "memory.md"};
    for (const auto& name : main_files) {
        std::string path = config_.workspace_dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            result.push_back(build_file_entry(path));
            break;  // Only one main memory file
        }
    }
    
    // Check memory/ directory
    std::string memory_dir = config_.workspace_dir + "/memory";
    DIR* dir = opendir(memory_dir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            
            // Only .md files
            if (name.length() > 3 && name.substr(name.length() - 3) == ".md") {
                std::string path = memory_dir + "/" + name;
                result.push_back(build_file_entry(path));
            }
        }
        closedir(dir);
    }
    
    return result;
}

MemoryFile MemoryManager::build_file_entry(const std::string& abs_path) {
    MemoryFile file;
    file.abs_path = abs_path;
    file.source = MemorySource::MEMORY;
    
    // Compute relative path
    if (abs_path.find(config_.workspace_dir) == 0) {
        file.path = abs_path.substr(config_.workspace_dir.length() + 1);
    } else {
        file.path = abs_path;
    }
    
    // Get file stats
    struct stat st;
    if (stat(abs_path.c_str(), &st) == 0) {
        file.mtime = static_cast<int64_t>(st.st_mtime) * 1000;
        file.size = st.st_size;
    }
    
    // Compute hash of content
    std::ifstream f(abs_path.c_str());
    if (f) {
        std::stringstream buffer;
        buffer << f.rdbuf();
        file.hash = compute_hash(buffer.str());
    }
    
    return file;
}

bool MemoryManager::index_file(const MemoryFile& file) {
    // Read file content
    std::ifstream f(file.abs_path.c_str());
    if (!f) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();
    
    // Delete existing chunks
    store_->delete_chunks_for_file(file.path, file.source);
    
    // Create new chunks
    std::vector<MemoryChunk> chunks = chunk_content(content, file.path, file.source);
    
    for (const auto& chunk : chunks) {
        if (!store_->upsert_chunk(chunk)) {
            // Log error but continue
        }
    }
    
    // Update file record
    return store_->upsert_file(file);
}

std::vector<MemoryChunk> MemoryManager::chunk_content(const std::string& content,
                                                       const std::string& path,
                                                       MemorySource source) {
    std::vector<MemoryChunk> chunks;
    
    // Split content into paragraphs first
    std::vector<std::string> paragraphs = split_into_paragraphs(content);
    
    // Combine paragraphs into chunks of target size
    int max_chars = config_.chunking.max_chars();
    int overlap_chars = config_.chunking.overlap_chars();
    
    std::string current_chunk;
    int chunk_start_line = 1;
    int line_count = 1;
    
    for (size_t i = 0; i < paragraphs.size(); ++i) {
        const std::string& para = paragraphs[i];
        
        // Count lines in this paragraph
        int para_lines = 1;
        for (char c : para) {
            if (c == '\n') para_lines++;
        }
        
        // Check if adding this paragraph would exceed target size
        if (!current_chunk.empty() && 
            static_cast<int>(current_chunk.length() + para.length()) > max_chars) {
            // Save current chunk
            MemoryChunk chunk;
            chunk.id = generate_uuid();
            chunk.path = path;
            chunk.source = source;
            chunk.start_line = chunk_start_line;
            chunk.end_line = line_count - 1;
            chunk.text = current_chunk;
            chunk.hash = compute_hash(current_chunk);
            chunk.updated_at = get_current_timestamp();
            chunks.push_back(chunk);
            
            // Start new chunk with overlap
            if (overlap_chars > 0 && static_cast<int>(current_chunk.length()) > overlap_chars) {
                current_chunk = current_chunk.substr(current_chunk.length() - overlap_chars);
            } else {
                current_chunk.clear();
            }
            chunk_start_line = line_count;
        }
        
        if (!current_chunk.empty()) {
            current_chunk += "\n\n";
        }
        current_chunk += para;
        line_count += para_lines;
    }
    
    // Save final chunk if non-empty
    if (!current_chunk.empty()) {
        MemoryChunk chunk;
        chunk.id = generate_uuid();
        chunk.path = path;
        chunk.source = source;
        chunk.start_line = chunk_start_line;
        chunk.end_line = line_count;
        chunk.text = current_chunk;
        chunk.hash = compute_hash(current_chunk);
        chunk.updated_at = get_current_timestamp();
        chunks.push_back(chunk);
    }
    
    return chunks;
}

std::vector<std::string> MemoryManager::split_into_paragraphs(const std::string& content) {
    std::vector<std::string> paragraphs;
    std::string current;
    
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        bool is_blank = line.empty() || (line.find_first_not_of(" \t\r") == std::string::npos);
        
        if (is_blank) {
            if (!current.empty()) {
                paragraphs.push_back(current);
                current.clear();
            }
        } else {
            if (!current.empty()) {
                current += "\n";
            }
            current += line;
        }
    }
    
    if (!current.empty()) {
        paragraphs.push_back(current);
    }
    
    return paragraphs;
}

std::string MemoryManager::compute_hash(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.c_str()), 
           content.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string MemoryManager::generate_uuid() {
    // Simple UUID v4 generation
    static bool seeded = false;
    if (!seeded) {
        srand(static_cast<unsigned>(time(nullptr)));
        seeded = true;
    }
    
    char buf[37];
    snprintf(buf, sizeof(buf), 
             "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
             rand() & 0xffff, rand() & 0xffff,
             rand() & 0xffff,
             (rand() & 0x0fff) | 0x4000,
             (rand() & 0x3fff) | 0x8000,
             rand() & 0xffff, rand() & 0xffff, rand() & 0xffff);
    
    return std::string(buf);
}

std::string MemoryManager::get_today_date() {
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
    return std::string(buf);
}

int64_t MemoryManager::get_current_timestamp() {
    return static_cast<int64_t>(time(nullptr)) * 1000;
}

void MemoryManager::set_error(const std::string& error) {
    last_error_ = error;
}

} // namespace openclaw
