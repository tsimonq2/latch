#include "lock_state.hpp"

namespace latch {

LockMachine::LockMachine(bool require_lock)
    : require_lock_(require_lock)
{
}

void LockMachine::fail(int code, std::string message)
{
    if (exit_code_) {
        return;
    }
    exit_code_ = code;
    error_ = std::move(message);
}

void LockMachine::missing_global()
{
    fail(3, "compositor has no ext_session_lock_manager_v1");
}

void LockMachine::lock_granted(bool locked_event)
{
    if (locked_event) {
        saw_locked_ = true;
    }
}

void LockMachine::finished()
{
    if (saw_locked_) {
        // The XML says unlock_and_destroy once locked was received, even
        // when the compositor ended the lock itself.
        unlock_sent_ = true;
        return;
    }
    fail(require_lock_ ? 1 : 0, require_lock_ ? "another client holds the lock" : "");
}

void LockMachine::output_appeared(uint32_t output)
{
    auto& slot = outputs_[output];
    slot.removed = false;
}

void LockMachine::output_removed(uint32_t output)
{
    auto it = outputs_.find(output);
    if (it == outputs_.end()) {
        return;
    }
    it->second.removed = true;
    it->second.surface = false;
    consider_secure();
}

bool LockMachine::note_surface(uint32_t output)
{
    auto& slot = outputs_[output];
    if (slot.surface) {
        fail(2, "duplicate lock surface");
        return false;
    }
    slot.surface = true;
    slot.removed = false;
    return true;
}

void LockMachine::configure(uint32_t output, uint32_t serial, int width, int height)
{
    auto& slot = outputs_[output];
    slot.have_configure = true;
    slot.serial = serial;
    slot.width = width;
    slot.height = height;
    slot.acked = false;
}

bool LockMachine::ack(uint32_t output, uint32_t serial)
{
    auto it = outputs_.find(output);
    if (it == outputs_.end() || !it->second.have_configure) {
        fail(2, "ack without configure");
        return false;
    }
    if (it->second.acked && it->second.acked_serial == serial) {
        fail(2, "ack of a configure that was already acked");
        return false;
    }
    if (serial != it->second.serial) {
        fail(2, "ack of an old serial");
        return false;
    }
    it->second.acked = true;
    it->second.acked_serial = serial;
    return true;
}

bool LockMachine::commit(uint32_t output, int width, int height, bool null_buffer)
{
    if (null_buffer) {
        fail(2, "null buffer");
        return false;
    }
    auto it = outputs_.find(output);
    if (it == outputs_.end() || !it->second.acked) {
        fail(2, "commit before ack");
        return false;
    }
    if (width != it->second.width || height != it->second.height) {
        fail(2, "buffer size does not match the acked configure");
        return false;
    }
    it->second.committed = true;
    consider_secure();
    return true;
}

bool LockMachine::request_unlock()
{
    if (!saw_locked_) {
        error_ = "unlock before locked";
        return false;
    }
    unlock_sent_ = true;
    return true;
}

bool LockMachine::request_destroy()
{
    if (saw_locked_) {
        error_ = "destroy after locked";
        return false;
    }
    if (!exit_code_) {
        exit_code_ = 0;
    }
    return true;
}

void LockMachine::unlock_already_gone()
{
    // Mir raises a protocol error if we unlock after it already cleared the
    // slot. The session is unlocked either way.
    exit_code_ = 0;
    error_.clear();
}

void LockMachine::consider_secure()
{
    if (secure_latched_ || !saw_locked_) {
        return;
    }
    if (outputs_.empty()) {
        return;
    }
    for (const auto& [_, slot] : outputs_) {
        if (slot.removed) {
            continue;
        }
        if (!slot.committed) {
            return;
        }
    }
    secure_latched_ = true;
}

bool LockMachine::secure() const
{
    return secure_latched_;
}

bool LockMachine::saw_locked() const
{
    return saw_locked_;
}

bool LockMachine::failed() const
{
    return exit_code_ && *exit_code_ != 0;
}

std::optional<int> LockMachine::exit_code() const
{
    return exit_code_;
}

const std::string& LockMachine::error() const
{
    return error_;
}

const std::map<uint32_t, OutputState>& LockMachine::outputs() const
{
    return outputs_;
}

bool SlotSim::take()
{
    if (slot_taken_) {
        return false;
    }
    slot_taken_ = true;
    scene_locked_ = true;
    return true;
}

void SlotSim::client_died()
{
    slot_taken_ = false;
}

bool SlotSim::scene_locked() const
{
    return scene_locked_;
}

bool SlotSim::slot_taken() const
{
    return slot_taken_;
}

} // namespace latch
