/*
 * OpenClaw C++11 - Built-in Agent Tools
 * 
 * Provides filesystem and shell tools for the agent:
 * - read: Read file contents
 * - write: Write content to files
 * - bash: Execute shell commands
 * - list_dir: List directory contents
 * - content_chunk: Retrieve chunks of large content
 * - content_search: Search within large content
 */
#ifndef OPENCLAW_CORE_BUILTIN_TOOLS_HPP
#define OPENCLAW_CORE_BUILTIN_TOOLS_HPP

#include "agent.hpp"
#include <string>

namespace openclaw {
namespace builtin_tools {

// ============================================================================
// Tool Factory Functions
// ============================================================================

/**
 * Create a file reading tool.
 * 
 * @param workspace_dir Base directory for relative paths
 * @return Configured AgentTool
 */
AgentTool create_read_tool(const std::string& workspace_dir);

/**
 * Create a file writing tool.
 * 
 * @param workspace_dir Base directory for relative paths
 * @return Configured AgentTool
 */
AgentTool create_write_tool(const std::string& workspace_dir);

/**
 * Create a bash/shell execution tool.
 * 
 * @param workspace_dir Working directory for commands
 * @param timeout_secs Command timeout (default: 20 seconds)
 * @return Configured AgentTool
 */
AgentTool create_bash_tool(const std::string& workspace_dir, int timeout_secs = 20);

/**
 * Create a directory listing tool.
 * 
 * @param workspace_dir Base directory for relative paths
 * @return Configured AgentTool
 */
AgentTool create_list_dir_tool(const std::string& workspace_dir);

/**
 * Create a content chunk retrieval tool.
 * 
 * Used to access parts of large content that was chunked
 * due to size limits.
 * 
 * @param chunker Reference to the ContentChunker instance
 * @return Configured AgentTool
 */
AgentTool create_content_chunk_tool(ContentChunker& chunker);

/**
 * Create a content search tool.
 * 
 * Searches within large chunked content without reading all chunks.
 * 
 * @param chunker Reference to the ContentChunker instance
 * @return Configured AgentTool
 */
AgentTool create_content_search_tool(ContentChunker& chunker);

// ============================================================================
// Path Utilities
// ============================================================================

namespace path {

/**
 * Resolve a path relative to workspace.
 * Handles both absolute and relative paths.
 */
std::string resolve(const std::string& path, const std::string& workspace);

/**
 * Check if a path is within the workspace (security check).
 * Prevents directory traversal attacks.
 */
bool is_within_workspace(const std::string& path, const std::string& workspace);

} // namespace path

} // namespace builtin_tools
} // namespace openclaw

#endif // OPENCLAW_CORE_BUILTIN_TOOLS_HPP
