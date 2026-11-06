#pragma once

#include "compiler.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace latch {

// Pure record of what the client has been told and what it is allowed to
// send. The compositor still owns blanking and "stay locked if we die."
struct OutputState {
    bool surface = false;
    bool removed = false;
    uint32_t serial = 0;
    bool have_configure = false;
    bool acked = false;
    uint32_t acked_serial = 0;
    int width = 0;
    int height = 0;
    bool committed = false;
};

class LockMachine {
public:
    explicit LockMachine(bool require_lock = false);

    void missing_global();
    void lock_granted(bool locked_event);
    void finished();
    void configure(uint32_t output, uint32_t serial, int width, int height);
    void output_appeared(uint32_t output);
    void output_removed(uint32_t output);
    // One lock surface per output. A second one is a client bug.
    bool note_surface(uint32_t output);
    bool ack(uint32_t output, uint32_t serial);
    bool commit(uint32_t output, int width, int height, bool null_buffer);
    bool request_unlock();
    bool request_destroy();
    // The compositor already dropped the lock, and our unlock request errored.
    void unlock_already_gone();

    bool secure() const;
    bool saw_locked() const;
    bool failed() const;
    std::optional<int> exit_code() const;
    const std::string& error() const;
    const std::map<uint32_t, OutputState>& outputs() const;

private:
    void fail(int code, std::string message);
    void consider_secure();

    bool require_lock_ = false;
    bool saw_locked_ = false;
    bool secure_latched_ = false;
    bool unlock_sent_ = false;
    std::optional<int> exit_code_;
    std::string error_;
    std::map<uint32_t, OutputState> outputs_;
};

// The compositor side of "the client died." The scene stays locked and the
// next client is allowed to take the empty slot.
class SlotSim {
public:
    bool take();
    void client_died();
    bool scene_locked() const;
    bool slot_taken() const;

private:
    bool scene_locked_ = false;
    bool slot_taken_ = false;
};

} // namespace latch
