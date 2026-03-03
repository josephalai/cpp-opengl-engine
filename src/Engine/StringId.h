// src/Engine/StringId.h
// Compile-time / load-time hashed string identifier.
// Replaces std::string keys in hot-path data structures (e.g. AnimationController).
//
// Usage:
//   constexpr StringId kIdle("Idle");
//   StringId dynId(someStdString);
//   map[kIdle.value()] = ...;
//
// Debug: keep a reverse lookup (StringId::debugName()) in non-Release builds.

#ifndef ENGINE_STRINGID_H
#define ENGINE_STRINGID_H

#include <cstdint>
#include <string>
#include <ostream>

#ifndef NDEBUG
#include <unordered_map>
#endif

class StringId {
public:
    constexpr StringId() : hash_(0) {}

    constexpr explicit StringId(const char* str)
        : hash_(fnv1a(str)) {}

    explicit StringId(const std::string& str)
        : hash_(fnv1a(str.c_str())) {
#ifndef NDEBUG
        registerName(hash_, str);
#endif
    }

    bool operator==(const StringId& o) const { return hash_ == o.hash_; }
    bool operator!=(const StringId& o) const { return hash_ != o.hash_; }
    bool operator< (const StringId& o) const { return hash_ <  o.hash_; }

    uint32_t value() const { return hash_; }

    friend std::ostream& operator<<(std::ostream& os, const StringId& id) {
#ifndef NDEBUG
        const std::string* name = lookupName(id.hash_);
        if (name) return os << *name;
#endif
        return os << id.hash_;
    }

#ifndef NDEBUG
    /// Returns the original string registered for this hash, or nullptr.
    static const std::string* lookupName(uint32_t hash) {
        auto& m = reverseMap();
        auto it = m.find(hash);
        return it != m.end() ? &it->second : nullptr;
    }
#endif

private:
    uint32_t hash_;

    static constexpr uint32_t fnv1a(const char* s,
                                     uint32_t h = 2166136261u) {
        return *s ? fnv1a(s + 1, (h ^ static_cast<uint32_t>(*s)) * 16777619u) : h;
    }

#ifndef NDEBUG
    static std::unordered_map<uint32_t, std::string>& reverseMap() {
        static std::unordered_map<uint32_t, std::string> m;
        return m;
    }
    static void registerName(uint32_t hash, const std::string& name) {
        reverseMap().emplace(hash, name);
    }
#endif
};

// Allow StringId to be used as key in std::unordered_map
namespace std {
    template<>
    struct hash<StringId> {
        size_t operator()(const StringId& id) const noexcept {
            return static_cast<size_t>(id.value());
        }
    };
}

#endif // ENGINE_STRINGID_H
