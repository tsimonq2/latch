#include "check.hpp"
#include "lock_state.hpp"

#include <iostream>

namespace {

void test_secure_point()
{
    latch::LockMachine machine;
    machine.lock_granted(true);
    CHECK(!machine.secure());
    machine.output_appeared(1);
    machine.output_appeared(2);
    CHECK(machine.note_surface(1));
    CHECK(machine.note_surface(2));
    machine.configure(1, 7, 100, 100);
    machine.configure(2, 8, 800, 600);
    CHECK(machine.ack(1, 7));
    CHECK(machine.commit(1, 100, 100, false));
    CHECK(!machine.secure());
    CHECK(machine.ack(2, 8));
    CHECK(machine.commit(2, 800, 600, false));
    CHECK(machine.secure());

    machine.output_appeared(3);
    CHECK(machine.note_surface(3));
    CHECK(machine.secure());
}

void test_illegal()
{
    latch::LockMachine machine;
    machine.output_appeared(1);
    CHECK(machine.note_surface(1));
    CHECK(!machine.note_surface(1));
    CHECK(machine.exit_code() == 2);

    latch::LockMachine early;
    early.output_appeared(1);
    early.note_surface(1);
    CHECK(!early.commit(1, 10, 10, false));
    CHECK(early.exit_code() == 2);

    latch::LockMachine nulls;
    nulls.output_appeared(1);
    nulls.note_surface(1);
    nulls.configure(1, 1, 10, 10);
    nulls.ack(1, 1);
    CHECK(!nulls.commit(1, 10, 10, true));
    CHECK(nulls.exit_code() == 2);

    latch::LockMachine size;
    size.output_appeared(1);
    size.note_surface(1);
    size.configure(1, 1, 10, 10);
    size.ack(1, 1);
    CHECK(!size.commit(1, 11, 10, false));
    CHECK(size.exit_code() == 2);

    latch::LockMachine serial;
    serial.output_appeared(1);
    serial.note_surface(1);
    serial.configure(1, 1, 10, 10);
    serial.configure(1, 2, 20, 20);
    CHECK(!serial.ack(1, 1));
    CHECK(serial.exit_code() == 2);

    latch::LockMachine unlock;
    CHECK(!unlock.request_unlock());
    CHECK(unlock.error().find("unlock before locked") != std::string::npos);

    latch::LockMachine destroy;
    destroy.lock_granted(true);
    CHECK(!destroy.request_destroy());
    CHECK(destroy.error().find("destroy after locked") != std::string::npos);
}

void test_finished()
{
    latch::LockMachine other(false);
    other.finished();
    CHECK(other.exit_code() == 0);

    latch::LockMachine exclusive(true);
    exclusive.finished();
    CHECK(exclusive.exit_code() == 1);

    latch::LockMachine ended;
    ended.lock_granted(true);
    ended.finished();
    CHECK(ended.request_unlock());
    ended.unlock_already_gone();
    CHECK(ended.exit_code() == 0);
}

void test_slot()
{
    latch::SlotSim slot;
    CHECK(slot.take());
    CHECK(slot.scene_locked());
    CHECK(!slot.take());
    slot.client_died();
    CHECK(slot.scene_locked());
    CHECK(!slot.slot_taken());
    CHECK(slot.take());
}

void test_missing_global()
{
    latch::LockMachine machine;
    machine.missing_global();
    CHECK(machine.exit_code() == 3);
}

} // namespace

int main()
{
    test_secure_point();
    test_illegal();
    test_finished();
    test_slot();
    test_missing_global();
    if (latch_failures != 0) {
        std::cerr << latch_failures << " checks failed\n";
        return 1;
    }
    return 0;
}
