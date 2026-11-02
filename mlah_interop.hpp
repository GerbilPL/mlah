#pragma once

#include <string>
#include <expected>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace mlah::interop {

    // --- Opaque Pointer Casts ---
    // Safely cast C++ class pointers to C structs and back
    
    template <typename CppObj, typename COpaque>
    inline COpaque* to_c_ptr(CppObj* ptr) noexcept {
        return reinterpret_cast<COpaque*>(ptr);
    }

    template <typename CppObj, typename COpaque>
    inline CppObj* to_cpp_ptr(COpaque* ptr) noexcept {
        return reinterpret_cast<CppObj*>(ptr);
    }

    // --- String Conversions ---

    // C++ to C (Allocates memory - consumer MUST free it!)
    inline char* allocate_c_string(const std::string& cpp_str) noexcept {
        char* c_str = static_cast<char*>(std::malloc(cpp_str.length() + 1));
        if (c_str) {
            std::memcpy(c_str, cpp_str.c_str(), cpp_str.length() + 1);
        }
        return c_str;
    }

    // Free memory allocated by allocate_c_string
    inline void free_c_string(char* c_str) noexcept {
        std::free(c_str);
    }

    // C to C++ (Safe conversion with fallback)
    inline std::string to_cpp_string(const char* c_str, const std::string& fallback = "") {
        return c_str ? std::string(c_str) : fallback;
    }

    // --- Expected / Result Pattern ---
    // Helper to unwrap C-style result structs back to std::expected.
    // Assumes C struct looks like: { bool success; ValueType val; ErrorType err; }
    
    template <typename ExpectedType, typename CResultStruct>
    inline ExpectedType from_c_result(const CResultStruct& c_res) {
        if (c_res.success) {
            return ExpectedType(std::in_place, c_res.val);
        } else {
            return std::unexpected(c_res.err);
        }
    }

} // namespace mlah::interop