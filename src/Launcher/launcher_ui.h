#pragma once

#include "config.h"

namespace KenshiMP {

/// Show launcher dialog. Returns true if user chose to launch, false to exit.
/// Updates config with user choices.
bool ShowLauncherDialog(LauncherConfig& config);

} // namespace KenshiMP
