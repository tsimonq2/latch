#pragma once

#include "compiler.hpp"

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace latch {

// Where a value came from. Later layers win per key. System is the fallback
// for anything the user file does not set.
enum class Origin { Default, System, User };

enum class Form { Focused, All };
enum class OutputForm { Field, Clock };

struct OutputOverride {
    std::optional<std::string> image;
    std::optional<std::string> background;
    std::optional<std::string> foreground;
    std::optional<std::string> accent;
    std::optional<std::string> error;
    std::optional<OutputForm> form;
};

struct Config {
    int version = 1;
    struct Auth {
        std::string pam_service = "latch";
        bool reject_empty = true;
        int fail_delay_ms = 0;
    } auth;
    struct Logind {
        bool inhibit_sleep = true;
        bool inhibit_shutdown = true;
        bool relock_on_resume = true;
        bool honor_unlock_signal = false;
        bool emit_unlock_signal = false;
    } logind;
    struct Idle {
        int timeout_sec = 0;
    } idle;
    struct Dbus {
        bool claim_screensaver = false;
    } dbus;
    struct Ui {
        Form form = Form::Focused;
        bool reveal = false;
        int cursor_hide_ms = 5000;
        std::string clock_format = "%H:%M";
        std::string date_format = "%A, %B %-d";
        std::string prompt = "Password";
        std::string fail_text = "Try again";
        std::string other_output_text = "Unlock on the keyboard screen";
        std::string font;
        std::string image;
        bool intro = true;
        std::map<std::string, OutputOverride> outputs;
        std::string background = "#1a1c1e";
        std::string foreground = "#f2f4f5";
        std::string accent = "#4c8dff";
        std::string error = "#ffb4ab";
    } ui;
};

struct LoadRequest {
    // When set, system and user directories are not read. Compiled defaults
    // still fill every key this file does not set.
    std::optional<std::filesystem::path> only_file;
    // Absolute path, or ignored when relative or unset.
    std::optional<std::filesystem::path> config_home;
    // Preference order: the first entry wins over the ones after it.
    std::vector<std::filesystem::path> config_dirs;
};

struct Loaded {
    Config config;
    std::map<std::string, Origin> origin;
};

struct ResolvedOutput {
    std::string image;
    std::string background;
    std::string foreground;
    std::string accent;
    std::string error;
    bool show_field = false;
};

std::expected<Loaded, std::string> load_config(const LoadRequest& request);

// XDG_CONFIG_HOME and XDG_CONFIG_DIRS. Relative values are ignored.
LoadRequest request_from_xdg(std::optional<std::filesystem::path> only_file,
                             std::string_view config_home,
                             bool home_set,
                             std::string_view config_dirs,
                             bool dirs_set);

// YAML containing only the keys where desired differs from baseline.
// The baseline is the next-lower layer (system over defaults, with no user file).
std::expected<std::string, std::string> sparse_overrides(const Config& desired, const Config& baseline);

std::string print_config(const Loaded& loaded, bool show_origin);

ResolvedOutput resolve_output(const Config& config, std::string_view output_name, bool keyboard_focused);

const char* origin_name(Origin origin);

} // namespace latch
