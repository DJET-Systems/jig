// SPDX-License-Identifier: GPL-3.0-only
// jig/commands.hpp
// One function per command. Each returns the process exit code and reports its own errors.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "jig-core/settings.hpp"
#include "jig-init/init.hpp"
#include "jig-ui/ui.hpp"

namespace jig::cmd {

int init(const Settings&, Ui&, const InitOptions&);

int toolchain_list(const Settings&, Ui&);
int toolchain_available(const Settings&, Ui&, const std::optional<std::string>& lang, const std::optional<std::string>& std);
int toolchain_which(const Settings&, Ui&, const std::optional<std::string>& tool);
int toolchain_use(const Settings&, Ui&, const std::string& spec);
int toolchain_pin(const Settings&, Ui&);
int not_implemented(Ui&, const std::string& what, const std::string& help);

int lock(const Settings&, Ui&, bool update, bool check);
int sync(const Settings&, Ui&, bool check);
int env(const Settings&, Ui&, const std::string& format);
int shell(const Settings&, Ui&);
int exec(const Settings&, Ui&, const std::vector<std::string>& argv);

int build(const Settings&, Ui&, const std::string& profile);
int run(const Settings&, Ui&, const std::string& profile, const std::vector<std::string>& args);
int test(const Settings&, Ui&, const std::string& profile, const std::vector<std::string>& args);
int fmt(const Settings&, Ui&, bool check);
int clean(const Settings&, Ui&, bool env, bool all);

int doctor(const Settings&, Ui&, bool strict);
int cache(const Settings&, Ui&, const std::string& action);
int version(const Settings&, Ui&);

}  // namespace jig::cmd
