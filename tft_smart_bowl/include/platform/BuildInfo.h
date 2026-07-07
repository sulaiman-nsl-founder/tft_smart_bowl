#ifndef PLATFORM_BUILD_INFO_H
#define PLATFORM_BUILD_INFO_H

namespace Platform {
namespace BuildInfo {

#ifndef FW_VERSION
#define FW_VERSION "0.1.0"
#endif

#ifndef GIT_REV
#define GIT_REV "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__ " " __TIME__
#endif

#ifndef BUILD_ENV
#define BUILD_ENV "debug"
#endif

#ifndef HW_REV
#define HW_REV "HW_REV_1"
#endif

constexpr const char* getVersion() { return FW_VERSION; }
constexpr const char* getGitRevision() { return GIT_REV; }
constexpr const char* getBuildDate() { return BUILD_DATE; }
constexpr const char* getEnvironment() { return BUILD_ENV; }
constexpr const char* getHardwareRevision() { return HW_REV; }

} // namespace BuildInfo
} // namespace Platform

#endif // PLATFORM_BUILD_INFO_H
