#include "config.hpp"

#include "contrast.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <sstream>

namespace latch {
namespace {

constexpr std::uintmax_t kMaxBytes = 256 * 1024;

std::expected<std::string, std::string> read_file(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::unexpected("cannot read " + path.string());
    }
    if (size > kMaxBytes) {
        return std::unexpected(path.string() + " is larger than 256 KiB");
    }
    std::ifstream in(path);
    if (!in) {
        return std::unexpected("cannot read " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::expected<void, std::string> check_depth(const YAML::Node& node, int depth)
{
    if (depth > 8) {
        return std::unexpected("config nesting is deeper than 8");
    }
    if (node.IsMap() || node.IsSequence()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            const YAML::Node& child = node.IsMap() ? it->second : *it;
            if (auto deeper = check_depth(child, depth + 1); !deeper) {
                return deeper;
            }
        }
    }
    return {};
}

std::expected<YAML::Node, std::string> parse_document(const std::filesystem::path& path)
{
    auto text = read_file(path);
    if (!text) {
        return std::unexpected(text.error());
    }
    YAML::Node node;
    try {
        node = YAML::Load(*text);
    } catch (const YAML::Exception& ex) {
        return std::unexpected(path.string() + ": " + ex.msg);
    }
    if (node.IsNull()) {
        return node;
    }
    if (!node.IsMap()) {
        return std::unexpected(path.string() + ": config must be a mapping");
    }
    if (auto depth = check_depth(node, 1); !depth) {
        return std::unexpected(path.string() + ": " + depth.error());
    }
    return node;
}

bool has_key(const YAML::Node& map, const char* key)
{
    return map[key] ? true : false;
}

template <typename T>
std::expected<T, std::string> as_type(const YAML::Node& node, const std::string& key)
{
    try {
        return node.as<T>();
    } catch (const YAML::Exception&) {
        return std::unexpected(key + " has the wrong type");
    }
}

std::expected<void, std::string> take_bool(const YAML::Node& map,
                                           const char* key,
                                           const std::string& origin_key,
                                           bool& dest,
                                           std::map<std::string, Origin>& origin,
                                           Origin who)
{
    if (!has_key(map, key)) {
        return {};
    }
    auto value = as_type<bool>(map[key], origin_key);
    if (!value) {
        return std::unexpected(value.error());
    }
    dest = *value;
    origin[origin_key] = who;
    return {};
}

std::expected<void, std::string> take_string(const YAML::Node& map,
                                             const char* key,
                                             const std::string& origin_key,
                                             std::string& dest,
                                             std::map<std::string, Origin>& origin,
                                             Origin who)
{
    if (!has_key(map, key)) {
        return {};
    }
    auto value = as_type<std::string>(map[key], origin_key);
    if (!value) {
        return std::unexpected(value.error());
    }
    dest = *value;
    origin[origin_key] = who;
    return {};
}

std::expected<void, std::string> take_int(const YAML::Node& map,
                                          const char* key,
                                          const std::string& origin_key,
                                          int& dest,
                                          std::map<std::string, Origin>& origin,
                                          Origin who)
{
    if (!has_key(map, key)) {
        return {};
    }
    auto value = as_type<int>(map[key], origin_key);
    if (!value) {
        return std::unexpected(value.error());
    }
    if (*value < 0) {
        return std::unexpected(origin_key + " must not be negative");
    }
    dest = *value;
    origin[origin_key] = who;
    return {};
}

std::expected<void, std::string> take_color(const YAML::Node& map,
                                            const char* key,
                                            const std::string& origin_key,
                                            std::string& dest,
                                            std::map<std::string, Origin>& origin,
                                            Origin who)
{
    if (!has_key(map, key)) {
        return {};
    }
    auto value = as_type<std::string>(map[key], origin_key);
    if (!value) {
        return std::unexpected(value.error());
    }
    if (auto color = parse_color(*value); !color) {
        return std::unexpected(origin_key + ": " + color.error());
    }
    dest = *value;
    origin[origin_key] = who;
    return {};
}

std::expected<void, std::string> reject_unknown(const YAML::Node& map,
                                                const std::vector<const char*>& known,
                                                const std::string& where)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        const auto key = it->first.as<std::string>();
        bool ok = false;
        for (const char* known_key : known) {
            if (key == known_key) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            return std::unexpected(where + " has unknown key " + key);
        }
    }
    return {};
}

std::expected<void, std::string> apply_output(OutputOverride& over,
                                              const YAML::Node& node,
                                              const std::string& prefix,
                                              std::map<std::string, Origin>& origin,
                                              Origin who)
{
    if (!node.IsMap()) {
        return std::unexpected(prefix + " must be a mapping");
    }
    if (auto unknown = reject_unknown(
            node, {"image", "background", "foreground", "accent", "error", "form"}, prefix);
        !unknown) {
        return unknown;
    }
    if (has_key(node, "image")) {
        auto value = as_type<std::string>(node["image"], prefix + ".image");
        if (!value) {
            return std::unexpected(value.error());
        }
        over.image = *value;
        origin[prefix + ".image"] = who;
    }
    auto color = [&](const char* key, std::optional<std::string>& dest) -> std::expected<void, std::string> {
        if (!has_key(node, key)) {
            return {};
        }
        const std::string full = prefix + "." + key;
        auto value = as_type<std::string>(node[key], full);
        if (!value) {
            return std::unexpected(value.error());
        }
        if (auto parsed = parse_color(*value); !parsed) {
            return std::unexpected(full + ": " + parsed.error());
        }
        dest = *value;
        origin[full] = who;
        return {};
    };
    if (auto step = color("background", over.background); !step) {
        return step;
    }
    if (auto step = color("foreground", over.foreground); !step) {
        return step;
    }
    if (auto step = color("accent", over.accent); !step) {
        return step;
    }
    if (auto step = color("error", over.error); !step) {
        return step;
    }
    if (has_key(node, "form")) {
        auto value = as_type<std::string>(node["form"], prefix + ".form");
        if (!value) {
            return std::unexpected(value.error());
        }
        if (*value == "field") {
            over.form = OutputForm::Field;
        } else if (*value == "clock") {
            over.form = OutputForm::Clock;
        } else {
            return std::unexpected(prefix + ".form must be field or clock");
        }
        origin[prefix + ".form"] = who;
    }
    return {};
}

std::expected<void, std::string> apply_document(Config& config,
                                                std::map<std::string, Origin>& origin,
                                                const YAML::Node& root,
                                                Origin who,
                                                const std::filesystem::path& path)
{
    if (root.IsNull()) {
        return {};
    }
    if (auto unknown = reject_unknown(root, {"version", "auth", "logind", "idle", "dbus", "ui"},
                                      path.string());
        !unknown) {
        return unknown;
    }
    const bool any_setting = has_key(root, "auth") || has_key(root, "logind") || has_key(root, "idle") ||
                             has_key(root, "dbus") || has_key(root, "ui") || has_key(root, "version");
    if (any_setting && !has_key(root, "version")) {
        return std::unexpected(path.string() + ": version is required");
    }
    if (has_key(root, "version")) {
        auto version = as_type<int>(root["version"], "version");
        if (!version) {
            return std::unexpected(version.error());
        }
        if (*version != 1) {
            return std::unexpected("unsupported config version");
        }
        config.version = *version;
        origin["version"] = who;
    }

    if (has_key(root, "auth")) {
        const YAML::Node auth = root["auth"];
        if (!auth.IsMap()) {
            return std::unexpected("auth must be a mapping");
        }
        if (auto unknown = reject_unknown(auth, {"pam_service", "reject_empty", "fail_delay_ms"}, "auth");
            !unknown) {
            return unknown;
        }
        if (auto step = take_string(auth, "pam_service", "auth.pam_service", config.auth.pam_service, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_bool(auth, "reject_empty", "auth.reject_empty", config.auth.reject_empty, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_int(auth, "fail_delay_ms", "auth.fail_delay_ms", config.auth.fail_delay_ms, origin, who);
            !step) {
            return step;
        }
    }
    if (has_key(root, "logind")) {
        const YAML::Node logind = root["logind"];
        if (!logind.IsMap()) {
            return std::unexpected("logind must be a mapping");
        }
        if (auto unknown = reject_unknown(logind,
                                           {"inhibit_sleep", "inhibit_shutdown", "relock_on_resume",
                                            "honor_unlock_signal", "emit_unlock_signal"},
                                           "logind");
            !unknown) {
            return unknown;
        }
        if (auto step = take_bool(logind, "inhibit_sleep", "logind.inhibit_sleep", config.logind.inhibit_sleep,
                                  origin, who);
            !step) {
            return step;
        }
        if (auto step = take_bool(logind, "inhibit_shutdown", "logind.inhibit_shutdown",
                                  config.logind.inhibit_shutdown, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_bool(logind, "relock_on_resume", "logind.relock_on_resume",
                                  config.logind.relock_on_resume, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_bool(logind, "honor_unlock_signal", "logind.honor_unlock_signal",
                                  config.logind.honor_unlock_signal, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_bool(logind, "emit_unlock_signal", "logind.emit_unlock_signal",
                                  config.logind.emit_unlock_signal, origin, who);
            !step) {
            return step;
        }
    }
    if (has_key(root, "idle")) {
        const YAML::Node idle = root["idle"];
        if (!idle.IsMap()) {
            return std::unexpected("idle must be a mapping");
        }
        if (auto unknown = reject_unknown(idle, {"timeout_sec"}, "idle"); !unknown) {
            return unknown;
        }
        if (auto step = take_int(idle, "timeout_sec", "idle.timeout_sec", config.idle.timeout_sec, origin, who);
            !step) {
            return step;
        }
    }
    if (has_key(root, "dbus")) {
        const YAML::Node dbus = root["dbus"];
        if (!dbus.IsMap()) {
            return std::unexpected("dbus must be a mapping");
        }
        if (auto unknown = reject_unknown(dbus, {"claim_screensaver"}, "dbus"); !unknown) {
            return unknown;
        }
        if (auto step = take_bool(dbus, "claim_screensaver", "dbus.claim_screensaver",
                                  config.dbus.claim_screensaver, origin, who);
            !step) {
            return step;
        }
    }
    if (has_key(root, "ui")) {
        const YAML::Node ui = root["ui"];
        if (!ui.IsMap()) {
            return std::unexpected("ui must be a mapping");
        }
        if (auto unknown = reject_unknown(ui,
                                           {"form", "reveal", "cursor_hide_ms", "clock_format", "date_format",
                                            "prompt", "fail_text", "other_output_text", "font", "image", "intro",
                                            "outputs", "background", "foreground", "accent", "error"},
                                           "ui");
            !unknown) {
            return unknown;
        }
        if (has_key(ui, "form")) {
            auto value = as_type<std::string>(ui["form"], "ui.form");
            if (!value) {
                return std::unexpected(value.error());
            }
            if (*value == "focused") {
                config.ui.form = Form::Focused;
            } else if (*value == "all") {
                config.ui.form = Form::All;
            } else {
                return std::unexpected("ui.form must be focused or all");
            }
            origin["ui.form"] = who;
        }
        if (auto step = take_bool(ui, "reveal", "ui.reveal", config.ui.reveal, origin, who); !step) {
            return step;
        }
        if (auto step = take_int(ui, "cursor_hide_ms", "ui.cursor_hide_ms", config.ui.cursor_hide_ms, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_string(ui, "clock_format", "ui.clock_format", config.ui.clock_format, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_string(ui, "date_format", "ui.date_format", config.ui.date_format, origin, who);
            !step) {
            return step;
        }
        if (auto step = take_string(ui, "prompt", "ui.prompt", config.ui.prompt, origin, who); !step) {
            return step;
        }
        if (auto step = take_string(ui, "fail_text", "ui.fail_text", config.ui.fail_text, origin, who); !step) {
            return step;
        }
        if (auto step = take_string(ui, "other_output_text", "ui.other_output_text", config.ui.other_output_text,
                                    origin, who);
            !step) {
            return step;
        }
        if (auto step = take_string(ui, "font", "ui.font", config.ui.font, origin, who); !step) {
            return step;
        }
        if (auto step = take_string(ui, "image", "ui.image", config.ui.image, origin, who); !step) {
            return step;
        }
        if (auto step = take_bool(ui, "intro", "ui.intro", config.ui.intro, origin, who); !step) {
            return step;
        }
        if (auto step = take_color(ui, "background", "ui.background", config.ui.background, origin, who); !step) {
            return step;
        }
        if (auto step = take_color(ui, "foreground", "ui.foreground", config.ui.foreground, origin, who); !step) {
            return step;
        }
        if (auto step = take_color(ui, "accent", "ui.accent", config.ui.accent, origin, who); !step) {
            return step;
        }
        if (auto step = take_color(ui, "error", "ui.error", config.ui.error, origin, who); !step) {
            return step;
        }
        if (has_key(ui, "outputs")) {
            const YAML::Node outputs = ui["outputs"];
            if (!outputs.IsMap()) {
                return std::unexpected("ui.outputs must be a mapping");
            }
            for (auto it = outputs.begin(); it != outputs.end(); ++it) {
                const auto name = it->first.as<std::string>();
                if (auto step = apply_output(config.ui.outputs[name], it->second, "ui.outputs." + name, origin, who);
                    !step) {
                    return step;
                }
            }
        }
    }
    return {};
}

std::expected<void, std::string> apply_path(Config& config,
                                            std::map<std::string, Origin>& origin,
                                            const std::filesystem::path& file,
                                            Origin who)
{
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return {};
    }
    auto document = parse_document(file);
    if (!document) {
        return std::unexpected(document.error());
    }
    return apply_document(config, origin, *document, who, file);
}

void emit_bool(YAML::Emitter& out, const char* key, bool value)
{
    out << YAML::Key << key << YAML::Value << value;
}

void emit_string(YAML::Emitter& out, const char* key, const std::string& value)
{
    out << YAML::Key << key << YAML::Value << value;
}

bool same_output(const OutputOverride& left, const OutputOverride& right)
{
    return left.image == right.image && left.background == right.background &&
           left.foreground == right.foreground && left.accent == right.accent && left.error == right.error &&
           left.form == right.form;
}

} // namespace

const char* origin_name(Origin origin)
{
    switch (origin) {
    case Origin::Default:
        return "default";
    case Origin::System:
        return "system";
    case Origin::User:
        return "user";
    }
    return "default";
}

std::expected<Loaded, std::string> load_config(const LoadRequest& request)
{
    Loaded loaded;
    if (request.only_file) {
        if (auto step = apply_path(loaded.config, loaded.origin, *request.only_file, Origin::User); !step) {
            return std::unexpected(step.error());
        }
        return loaded;
    }
    // Right to left, so the first listed directory is applied last and wins.
    for (auto it = request.config_dirs.rbegin(); it != request.config_dirs.rend(); ++it) {
        if (it->empty() || it->is_relative()) {
            continue;
        }
        const auto file = *it / "latch" / "config.yaml";
        if (auto step = apply_path(loaded.config, loaded.origin, file, Origin::System); !step) {
            return std::unexpected(step.error());
        }
    }
    if (request.config_home && request.config_home->is_absolute()) {
        const auto file = *request.config_home / "latch" / "config.yaml";
        if (auto step = apply_path(loaded.config, loaded.origin, file, Origin::User); !step) {
            return std::unexpected(step.error());
        }
    }
    return loaded;
}

LoadRequest request_from_xdg(std::optional<std::filesystem::path> only_file,
                             std::string_view config_home,
                             bool home_set,
                             std::string_view config_dirs,
                             bool dirs_set)
{
    LoadRequest request;
    request.only_file = std::move(only_file);
    if (home_set) {
        const std::filesystem::path home{std::string(config_home)};
        if (home.is_absolute()) {
            request.config_home = home;
        }
    } else {
        // The caller fills this from $HOME when the variable is unset.
    }
    if (!dirs_set || config_dirs.empty()) {
        request.config_dirs.push_back("/etc/xdg");
        return request;
    }
    std::string rest(config_dirs);
    while (!rest.empty()) {
        const auto cut = rest.find(':');
        const auto piece = rest.substr(0, cut);
        if (!piece.empty()) {
            const std::filesystem::path path{piece};
            if (path.is_absolute()) {
                request.config_dirs.push_back(path);
            }
        }
        if (cut == std::string::npos) {
            break;
        }
        rest.erase(0, cut + 1);
    }
    if (request.config_dirs.empty()) {
        request.config_dirs.push_back("/etc/xdg");
    }
    return request;
}

std::expected<std::string, std::string> sparse_overrides(const Config& desired, const Config& baseline)
{
    YAML::Emitter doc;
    doc << YAML::Comment("Written by Latch Settings.");
    doc << YAML::BeginMap;
    doc << YAML::Key << "version" << YAML::Value << 1;
    bool wrote = false;

    if (desired.auth.pam_service != baseline.auth.pam_service ||
        desired.auth.reject_empty != baseline.auth.reject_empty ||
        desired.auth.fail_delay_ms != baseline.auth.fail_delay_ms) {
        wrote = true;
        doc << YAML::Key << "auth" << YAML::Value << YAML::BeginMap;
        if (desired.auth.pam_service != baseline.auth.pam_service) {
            emit_string(doc, "pam_service", desired.auth.pam_service);
        }
        if (desired.auth.reject_empty != baseline.auth.reject_empty) {
            emit_bool(doc, "reject_empty", desired.auth.reject_empty);
        }
        if (desired.auth.fail_delay_ms != baseline.auth.fail_delay_ms) {
            doc << YAML::Key << "fail_delay_ms" << YAML::Value << desired.auth.fail_delay_ms;
        }
        doc << YAML::EndMap;
    }
    if (desired.logind.inhibit_sleep != baseline.logind.inhibit_sleep ||
        desired.logind.inhibit_shutdown != baseline.logind.inhibit_shutdown ||
        desired.logind.relock_on_resume != baseline.logind.relock_on_resume ||
        desired.logind.honor_unlock_signal != baseline.logind.honor_unlock_signal ||
        desired.logind.emit_unlock_signal != baseline.logind.emit_unlock_signal) {
        wrote = true;
        doc << YAML::Key << "logind" << YAML::Value << YAML::BeginMap;
        if (desired.logind.inhibit_sleep != baseline.logind.inhibit_sleep) {
            emit_bool(doc, "inhibit_sleep", desired.logind.inhibit_sleep);
        }
        if (desired.logind.inhibit_shutdown != baseline.logind.inhibit_shutdown) {
            emit_bool(doc, "inhibit_shutdown", desired.logind.inhibit_shutdown);
        }
        if (desired.logind.relock_on_resume != baseline.logind.relock_on_resume) {
            emit_bool(doc, "relock_on_resume", desired.logind.relock_on_resume);
        }
        if (desired.logind.honor_unlock_signal != baseline.logind.honor_unlock_signal) {
            emit_bool(doc, "honor_unlock_signal", desired.logind.honor_unlock_signal);
        }
        if (desired.logind.emit_unlock_signal != baseline.logind.emit_unlock_signal) {
            emit_bool(doc, "emit_unlock_signal", desired.logind.emit_unlock_signal);
        }
        doc << YAML::EndMap;
    }
    if (desired.idle.timeout_sec != baseline.idle.timeout_sec) {
        wrote = true;
        doc << YAML::Key << "idle" << YAML::Value << YAML::BeginMap;
        doc << YAML::Key << "timeout_sec" << YAML::Value << desired.idle.timeout_sec;
        doc << YAML::EndMap;
    }
    if (desired.dbus.claim_screensaver != baseline.dbus.claim_screensaver) {
        wrote = true;
        doc << YAML::Key << "dbus" << YAML::Value << YAML::BeginMap;
        emit_bool(doc, "claim_screensaver", desired.dbus.claim_screensaver);
        doc << YAML::EndMap;
    }

    const bool ui_top = desired.ui.form != baseline.ui.form || desired.ui.reveal != baseline.ui.reveal ||
                        desired.ui.cursor_hide_ms != baseline.ui.cursor_hide_ms ||
                        desired.ui.clock_format != baseline.ui.clock_format ||
                        desired.ui.date_format != baseline.ui.date_format || desired.ui.prompt != baseline.ui.prompt ||
                        desired.ui.fail_text != baseline.ui.fail_text ||
                        desired.ui.other_output_text != baseline.ui.other_output_text ||
                        desired.ui.font != baseline.ui.font || desired.ui.image != baseline.ui.image ||
                        desired.ui.intro != baseline.ui.intro || desired.ui.background != baseline.ui.background ||
                        desired.ui.foreground != baseline.ui.foreground || desired.ui.accent != baseline.ui.accent ||
                        desired.ui.error != baseline.ui.error;
    std::vector<std::string> output_names;
    for (const auto& [name, _] : desired.ui.outputs) {
        output_names.push_back(name);
    }
    for (const auto& [name, _] : baseline.ui.outputs) {
        if (desired.ui.outputs.find(name) == desired.ui.outputs.end()) {
            output_names.push_back(name);
        }
    }
    std::vector<std::string> changed_outputs;
    for (const auto& name : output_names) {
        const OutputOverride empty;
        const auto desired_it = desired.ui.outputs.find(name);
        const auto base_it = baseline.ui.outputs.find(name);
        const OutputOverride& left = desired_it == desired.ui.outputs.end() ? empty : desired_it->second;
        const OutputOverride& right = base_it == baseline.ui.outputs.end() ? empty : base_it->second;
        if (!same_output(left, right)) {
            changed_outputs.push_back(name);
        }
    }
    if (ui_top || !changed_outputs.empty()) {
        wrote = true;
        doc << YAML::Key << "ui" << YAML::Value << YAML::BeginMap;
        if (desired.ui.form != baseline.ui.form) {
            emit_string(doc, "form", desired.ui.form == Form::All ? "all" : "focused");
        }
        if (desired.ui.reveal != baseline.ui.reveal) {
            emit_bool(doc, "reveal", desired.ui.reveal);
        }
        if (desired.ui.cursor_hide_ms != baseline.ui.cursor_hide_ms) {
            doc << YAML::Key << "cursor_hide_ms" << YAML::Value << desired.ui.cursor_hide_ms;
        }
        if (desired.ui.clock_format != baseline.ui.clock_format) {
            emit_string(doc, "clock_format", desired.ui.clock_format);
        }
        if (desired.ui.date_format != baseline.ui.date_format) {
            emit_string(doc, "date_format", desired.ui.date_format);
        }
        if (desired.ui.prompt != baseline.ui.prompt) {
            emit_string(doc, "prompt", desired.ui.prompt);
        }
        if (desired.ui.fail_text != baseline.ui.fail_text) {
            emit_string(doc, "fail_text", desired.ui.fail_text);
        }
        if (desired.ui.other_output_text != baseline.ui.other_output_text) {
            emit_string(doc, "other_output_text", desired.ui.other_output_text);
        }
        if (desired.ui.font != baseline.ui.font) {
            emit_string(doc, "font", desired.ui.font);
        }
        if (desired.ui.image != baseline.ui.image) {
            emit_string(doc, "image", desired.ui.image);
        }
        if (desired.ui.intro != baseline.ui.intro) {
            emit_bool(doc, "intro", desired.ui.intro);
        }
        if (desired.ui.background != baseline.ui.background) {
            emit_string(doc, "background", desired.ui.background);
        }
        if (desired.ui.foreground != baseline.ui.foreground) {
            emit_string(doc, "foreground", desired.ui.foreground);
        }
        if (desired.ui.accent != baseline.ui.accent) {
            emit_string(doc, "accent", desired.ui.accent);
        }
        if (desired.ui.error != baseline.ui.error) {
            emit_string(doc, "error", desired.ui.error);
        }
        if (!changed_outputs.empty()) {
            doc << YAML::Key << "outputs" << YAML::Value << YAML::BeginMap;
            for (const auto& name : changed_outputs) {
                const OutputOverride empty;
                const auto it = desired.ui.outputs.find(name);
                const OutputOverride& over = it == desired.ui.outputs.end() ? empty : it->second;
                doc << YAML::Key << name << YAML::Value << YAML::BeginMap;
                if (over.image) {
                    emit_string(doc, "image", *over.image);
                }
                if (over.background) {
                    emit_string(doc, "background", *over.background);
                }
                if (over.foreground) {
                    emit_string(doc, "foreground", *over.foreground);
                }
                if (over.accent) {
                    emit_string(doc, "accent", *over.accent);
                }
                if (over.error) {
                    emit_string(doc, "error", *over.error);
                }
                if (over.form) {
                    emit_string(doc, "form", *over.form == OutputForm::Field ? "field" : "clock");
                }
                doc << YAML::EndMap;
            }
            doc << YAML::EndMap;
        }
        doc << YAML::EndMap;
    }
    doc << YAML::EndMap;
    if (!wrote) {
        return std::string{};
    }
    if (!doc.good()) {
        return std::unexpected(doc.GetLastError());
    }
    return std::string(doc.c_str()) + "\n";
}

std::string print_config(const Loaded& loaded, bool show_origin)
{
    const Config& c = loaded.config;
    std::ostringstream out;
    auto line = [&](const std::string& key, const std::string& value) {
        out << key << "=" << value;
        if (show_origin) {
            const auto it = loaded.origin.find(key);
            out << " " << origin_name(it == loaded.origin.end() ? Origin::Default : it->second);
        }
        out << "\n";
    };
    line("version", std::to_string(c.version));
    line("auth.pam_service", c.auth.pam_service);
    line("auth.reject_empty", c.auth.reject_empty ? "true" : "false");
    line("auth.fail_delay_ms", std::to_string(c.auth.fail_delay_ms));
    line("logind.inhibit_sleep", c.logind.inhibit_sleep ? "true" : "false");
    line("logind.inhibit_shutdown", c.logind.inhibit_shutdown ? "true" : "false");
    line("logind.relock_on_resume", c.logind.relock_on_resume ? "true" : "false");
    line("logind.honor_unlock_signal", c.logind.honor_unlock_signal ? "true" : "false");
    line("logind.emit_unlock_signal", c.logind.emit_unlock_signal ? "true" : "false");
    line("idle.timeout_sec", std::to_string(c.idle.timeout_sec));
    line("dbus.claim_screensaver", c.dbus.claim_screensaver ? "true" : "false");
    line("ui.form", c.ui.form == Form::All ? "all" : "focused");
    line("ui.reveal", c.ui.reveal ? "true" : "false");
    line("ui.cursor_hide_ms", std::to_string(c.ui.cursor_hide_ms));
    line("ui.clock_format", c.ui.clock_format);
    line("ui.date_format", c.ui.date_format);
    line("ui.prompt", c.ui.prompt);
    line("ui.fail_text", c.ui.fail_text);
    line("ui.other_output_text", c.ui.other_output_text);
    line("ui.font", c.ui.font);
    line("ui.image", c.ui.image);
    line("ui.intro", c.ui.intro ? "true" : "false");
    line("ui.background", c.ui.background);
    line("ui.foreground", c.ui.foreground);
    line("ui.accent", c.ui.accent);
    line("ui.error", c.ui.error);
    for (const auto& [name, over] : c.ui.outputs) {
        if (over.background) {
            line("ui.outputs." + name + ".background", *over.background);
        }
        if (over.image) {
            line("ui.outputs." + name + ".image", *over.image);
        }
    }
    return out.str();
}

ResolvedOutput resolve_output(const Config& config, std::string_view output_name, bool keyboard_focused)
{
    ResolvedOutput resolved;
    resolved.image = config.ui.image;
    resolved.background = config.ui.background;
    resolved.foreground = config.ui.foreground;
    resolved.accent = config.ui.accent;
    resolved.error = config.ui.error;
    const auto it = config.ui.outputs.find(std::string(output_name));
    const OutputOverride* over = it == config.ui.outputs.end() ? nullptr : &it->second;
    if (over) {
        if (over->image) {
            resolved.image = *over->image;
        }
        if (over->background) {
            resolved.background = *over->background;
        }
        if (over->foreground) {
            resolved.foreground = *over->foreground;
        }
        if (over->accent) {
            resolved.accent = *over->accent;
        }
        if (over->error) {
            resolved.error = *over->error;
        }
    }
    if (over && over->form) {
        resolved.show_field = *over->form == OutputForm::Field;
    } else if (config.ui.form == Form::All) {
        resolved.show_field = true;
    } else {
        resolved.show_field = keyboard_focused;
    }
    return resolved;
}

} // namespace latch
