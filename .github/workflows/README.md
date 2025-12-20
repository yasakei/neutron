# GitHub Actions Workflows

This directory contains CI/CD workflows for the Neutron programming language.

## Workflows

### 🔧 CI Workflow (`ci.yml`)

**Trigger:** On every push/PR to `main`, `release`, or `develop` branches

**What it does:**
- ✅ Builds Neutron on **Linux (Ubuntu)**, **macOS**, and **Windows (Visual Studio)**
- ✅ Runs the full test suite (`python3 run_tests.py`) on all platforms
- ✅ Uploads build artifacts for 7 days
- ✅ Verifies binary with `--version` check
- ✅ Fails if any platform fails

**Artifacts generated:**
- `neutron-linux-x64` - Linux binary + shared library
- `neutron-macos-x64` - macOS binary + shared library
- `neutron-windows-x64` - Windows binary + DLLs

---

### 📦 Release Workflow (`release.yml`)

**Trigger:** When you push a version tag like `v1.0.4` or `v1.0.5-alpha`

**What it does:**
- ✅ Creates a GitHub Release (draft if prerelease)
- ✅ Builds release binaries for all 3 platforms
- ✅ Packages binaries with README and LICENSE
- ✅ Uploads release assets:
  - `neutron-vX.X.X-linux-x64.tar.gz`
  - `neutron-vX.X.X-macos-x64.tar.gz`
  - `neutron-vX.X.X-windows-x64.zip`

**How to trigger a release:**

```bash
# Tag your commit
git tag v1.0.5-alpha
git push origin v1.0.5-alpha

# Or manually trigger via GitHub Actions UI
```

---

## Status Badges

Add these to your README.md:

```markdown
[![CI](https://github.com/yasakei/neutron/actions/workflows/ci.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/ci.yml)
[![Release](https://github.com/yasakei/neutron/actions/workflows/release.yml/badge.svg)](https://github.com/yasakei/neutron/actions/workflows/release.yml)
```

---

## Platform Details

### Linux (Ubuntu Latest)
- **Compiler:** GCC
- **Dependencies:** libcurl4-openssl-dev, libjsoncpp-dev
- **Package manager:** apt-get

### macOS (Latest)
- **Compiler:** Clang (Xcode)
- **Dependencies:** curl, jsoncpp
- **Package manager:** Homebrew

### Windows (Visual Studio)
- **Compiler:** MSVC
- **Dependencies:** vcpkg (curl, jsoncpp)
- **Package manager:** vcpkg

### Windows (MSYS2 MINGW64 - Alternative)
- **Compiler:** GCC (mingw-w64)
- **Dependencies:** mingw-w64-x86_64-{gcc,cmake,curl,jsoncpp}
- **Package manager:** pacman

---

## Troubleshooting

### If CI fails on a platform:

1. **Check the build logs** in the Actions tab
2. **Reproduce locally** using the same commands from BUILD.md
3. **Common issues:**
   - Missing dependencies (check apt/brew/pacman install commands)
   - CMake configuration errors (verify CMakeLists.txt)
   - Test failures (check test output in logs)

### Testing workflows locally:

Use [act](https://github.com/nektos/act) to test GitHub Actions locally:

```bash
# Install act
brew install act  # macOS
# or: curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Run CI workflow
act -j build-linux

# Run all jobs
act push
```

---

## Maintenance

- **Update dependencies:** Modify the install steps in each job
- **Add new platforms:** Create a new job following the existing pattern
- **Change test command:** Update the "Run tests" step
- **Adjust retention:** Change `retention-days` in upload-artifact steps

---

## Questions?

See [BUILD.md](../docs/guides/BUILD.md) for detailed build instructions for each platform.
