#include "check.hpp"
#include "config.hpp"
#include "contrast.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace {

fs::path make_tree()
{
    const auto root = fs::temp_directory_path() / "latch-config-test";
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void write_file(const fs::path& path, std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

void test_defaults_and_contrast()
{
    const auto loaded = latch::load_config({});
    CHECK(loaded.has_value());
    CHECK(loaded->config.ui.prompt == "Password");
    CHECK(loaded->config.auth.reject_empty);
    CHECK(loaded->config.idle.timeout_sec == 0);
    const auto fg = latch::parse_color(loaded->config.ui.foreground);
    const auto bg = latch::parse_color(loaded->config.ui.background);
    const auto err = latch::parse_color(loaded->config.ui.error);
    CHECK(fg.has_value());
    CHECK(bg.has_value());
    CHECK(err.has_value());
    CHECK(latch::contrast_ratio(*fg, *bg) >= 4.5);
    CHECK(latch::contrast_ratio(*err, *bg) >= 4.5);
    const auto printed = latch::print_config(*loaded, true);
    CHECK(printed.find("ui.prompt=Password default") != std::string::npos);
}

void test_layers(const fs::path& root)
{
    const auto system_a = root / "a";
    const auto system_b = root / "b";
    const auto home = root / "home";
    write_file(system_a / "latch" / "config.yaml",
               "version: 1\nui:\n  background: \"#111111\"\n  prompt: \"From A\"\n");
    write_file(system_b / "latch" / "config.yaml",
               "version: 1\nui:\n  background: \"#222222\"\n  prompt: \"From B\"\n");
    latch::LoadRequest dirs;
    dirs.config_dirs = {system_a, system_b};
    const auto first_wins = latch::load_config(dirs);
    CHECK(first_wins.has_value());
    CHECK(first_wins->config.ui.background == "#111111");
    CHECK(first_wins->config.ui.prompt == "From A");
    CHECK(first_wins->origin.at("ui.background") == latch::Origin::System);

    write_file(home / "latch" / "config.yaml", "version: 1\nui:\n  background: \"#333333\"\n");
    latch::LoadRequest both;
    both.config_dirs = {system_a};
    both.config_home = home;
    const auto merged = latch::load_config(both);
    CHECK(merged.has_value());
    CHECK(merged->config.ui.background == "#333333");
    CHECK(merged->config.ui.prompt == "From A");
    CHECK(merged->origin.at("ui.prompt") == latch::Origin::System);
    CHECK(merged->origin.at("ui.background") == latch::Origin::User);

    latch::LoadRequest missing_user;
    missing_user.config_dirs = {system_a};
    missing_user.config_home = root / "no-such-home";
    const auto system_only = latch::load_config(missing_user);
    CHECK(system_only.has_value());
    CHECK(system_only->config.ui.prompt == "From A");
    CHECK(system_only->config.ui.background == "#111111");
}

void test_outputs_and_sparse(const fs::path& root)
{
    const auto system = root / "sys-out";
    const auto home = root / "home-out";
    write_file(system / "latch" / "config.yaml",
               "version: 1\nui:\n  prompt: \"System prompt\"\n  background: \"#111111\"\n"
               "  outputs:\n    DP-1:\n      background: \"#abcdef\"\n    NOT-A-REAL-OUTPUT:\n"
               "      background: \"#010203\"\n");
    latch::LoadRequest request;
    request.config_dirs = {system};
    const auto loaded = latch::load_config(request);
    CHECK(loaded.has_value());
    CHECK(loaded->config.ui.outputs.count("NOT-A-REAL-OUTPUT") == 1);
    const auto dp1 = latch::resolve_output(loaded->config, "DP-1", false);
    CHECK(dp1.background == "#abcdef");
    CHECK(loaded->config.ui.prompt == "System prompt");
    const auto other = latch::resolve_output(loaded->config, "eDP-1", true);
    CHECK(other.background == "#111111");
    CHECK(other.show_field);

    write_file(home / "latch" / "config.yaml", "version: 1\nui:\n  fail_text: \"Nope\"\n");
    latch::LoadRequest with_user = request;
    with_user.config_home = home;
    const auto effective = latch::load_config(with_user);
    CHECK(effective.has_value());
    CHECK(effective->config.ui.fail_text == "Nope");
    CHECK(effective->config.ui.prompt == "System prompt");

    latch::Config desired = effective->config;
    desired.ui.prompt = "Hello";
    const auto yaml = latch::sparse_overrides(desired, loaded->config);
    CHECK(yaml.has_value());
    CHECK(yaml->find("prompt") != std::string::npos);
    CHECK(yaml->find("fail_text") != std::string::npos);
    CHECK(yaml->find("background") == std::string::npos);

    desired.ui.prompt = loaded->config.ui.prompt;
    const auto dropped = latch::sparse_overrides(desired, loaded->config);
    CHECK(dropped.has_value());
    CHECK(dropped->find("prompt") == std::string::npos);
    CHECK(dropped->find("fail_text") != std::string::npos);
    write_file(home / "latch" / "config.yaml", *dropped);
    const auto restored = latch::load_config(with_user);
    CHECK(restored.has_value());
    CHECK(restored->config.ui.prompt == "System prompt");
    CHECK(restored->config.ui.fail_text == "Nope");
}

void test_errors(const fs::path& root)
{
    const auto file = root / "bad.yaml";
    write_file(file, "version: 2\n");
    latch::LoadRequest only;
    only.only_file = file;
    const auto bad_version = latch::load_config(only);
    CHECK(!bad_version.has_value());

    write_file(file, "version: 1\nui:\n  nope: 1\n");
    const auto unknown = latch::load_config(only);
    CHECK(!unknown.has_value());

    write_file(file, "ui:\n  prompt: \"x\"\n");
    const auto no_version = latch::load_config(only);
    CHECK(!no_version.has_value());

    std::string huge(256 * 1024 + 1, 'a');
    write_file(file, huge);
    const auto too_big = latch::load_config(only);
    CHECK(!too_big.has_value());

    const auto system = root / "ignored-system";
    write_file(system / "latch" / "config.yaml", "version: 1\nui:\n  prompt: \"System\"\n");
    write_file(file, "version: 1\nui:\n  prompt: \"File\"\n");
    latch::LoadRequest bypass;
    bypass.only_file = file;
    bypass.config_dirs = {system};
    const auto direct = latch::load_config(bypass);
    CHECK(direct.has_value());
    CHECK(direct->config.ui.prompt == "File");

    const auto xdg = latch::request_from_xdg(std::nullopt, "relative/home", true, "also-relative:/etc/xdg", true);
    CHECK(!xdg.config_home.has_value());
    CHECK(xdg.config_dirs.size() == 1);
    CHECK(xdg.config_dirs[0] == "/etc/xdg");
}

} // namespace

int main()
{
    const auto root = make_tree();
    test_defaults_and_contrast();
    test_layers(root);
    test_outputs_and_sparse(root);
    test_errors(root);
    fs::remove_all(root);
    if (latch_failures != 0) {
        std::cerr << latch_failures << " checks failed\n";
        return 1;
    }
    return 0;
}
