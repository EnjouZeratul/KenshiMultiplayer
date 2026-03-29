#pragma once

namespace KenshiMP {

/// Install unhandled exception filter for crash logging
void InstallExceptionHandler();

/// Uninstall
void UninstallExceptionHandler();

} // namespace KenshiMP
