# Maintaining the custom CrossPlay firmware

This fork keeps upstream CrossPlay, individual applications, and the firmware
installed on the device separate from one another:

- `upstream/xteink` is the original firmware from `ma-r-s/crossplay`.
- `origin/xteink` is the clean mirror in `jvfajardz/crossplay`.
- `origin/project-rules` carries the shared X4 Pro feasibility guide and is
  merged into every custom-app branch.
- `origin/app-<name>` contains one independently maintained application.
- `origin/custom-apps` combines the selected application branches into the
  firmware that is built and flashed.

## Branch structure

```text
xteink                  clean mirror of upstream/xteink
|-- project-rules       xteink + shared X4 Pro constraints
|-- app-transit         xteink + Transit only
|-- app-weather         xteink + Weather only (example)
`-- app-calendar        xteink + Calendar only (example)

custom-apps             xteink + every app selected for the device
```

New apps start from `xteink`, not from another app branch. An `app-<name>`
branch must contain only that app and the smallest necessary shelf/build wiring.
The device cannot combine branches while flashing, so `custom-apps` is the
integration branch used to produce a firmware containing several apps.

To add an app to the device firmware, merge its branch into `custom-apps`. To
remove an app from the device, rebuild `custom-apps` without that app; the
standalone app branch remains available and independently maintainable.

## Starting a new app from a new Codex task

The user may begin a new task with only:

> Let's create a new project.

Treat that phrase as a request to create another independent CrossPlay app for
the Xteink X4 Pro in this repository. Do not create a separate repository or
place the new app on top of Transit.

1. Read this entire file and inspect the current branches, worktrees, and remote
   state before changing anything.
2. Ask the user for the new app's name and one-sentence purpose if they were not
   supplied. Gather further requirements conversationally before committing to
   behavior that would materially affect the design.
3. Check whether `upstream/xteink` has advanced and report it. Do not rebase or
   publish merely because an update exists; follow the update workflow below
   when the user elects to adopt it.
4. Create `app-<name>` from the clean `xteink` branch, then merge
   `project-rules` before app development. Use a short lowercase kebab-case
   name, and refuse to overwrite an existing local or remote branch.
5. Keep the app isolated under `src/apps_local/<name>/`, with only the minimum
   shared shelf, build, font, or resource wiring needed by that app.
6. Build a realistic web preview early when the app has a visual interface, so
   layout and behavior can be reviewed before a device build.
7. Implement and verify the app on its standalone branch. Build only the
   explicit `x4pro` target and never publish an incomplete or failed build.
8. Publish a test installer only when requested. Do not commit generated files
   from `dist/`.
9. After the user confirms the hardware test, commit and push `app-<name>`, then
   merge that app into `custom-apps`, rebuild the combined firmware, and update
   its installer. Keep the standalone app branch independently usable.
10. Update this document when the new app adds permanent requirements, unusual
    data sources, build constraints, or maintenance steps future tasks need.

Unless the user says otherwise, “new project” means a new CrossPlay app within
the existing `xteink x4 pro` Codex project and repository—not a new Codex saved
project, filesystem folder, or GitHub repository.

## The only request needed

Ask Codex:

> Check if CrossPlay got an update.

Codex should perform the workflow below. If upstream has not advanced, it should
make no changes and report that the custom firmware is current.

## Update workflow

1. Fetch `upstream` and compare `upstream/xteink` with `origin/xteink`.
2. Review upstream release notes and commits for changes that could affect the
   X4 Pro, application shelf, networking, partition layout, or web flashing.
3. Fast-forward the local and fork `xteink` branches to `upstream/xteink`.
   Never add custom commits to `xteink`.
4. Rebase `project-rules` onto the updated `xteink` branch, resolve any changes
   to the upstream instruction chain, and push the updated rules branch.
5. Rebase each `app-<name>` branch onto the updated `xteink` branch, then merge
   the updated `project-rules`. Resolve
   conflicts by preserving upstream behavior and keeping each app as a small,
   isolated layer under `src/apps_local/`.
6. Recreate or rebase `custom-apps` from the updated `xteink` branch and merge
   the selected app branches into it. Do not develop an app directly on top of
   a different app branch.
7. Verify that Transit remains above Study in the Apps shelf and still includes:
   - bus 306 at Koogsingel in both directions;
   - metro 52 end to end in both directions;
   - bus 37 between Noord and Amstelstation in both directions, with the
     Noord departure window based on the next 306 departure plus 30 minutes;
   - NS between Purmerend Overwhere and Sloterdijk in both directions on
     non-Saturdays;
   - tram 26 between Amsterdam Centraal and Diemerparklaan in both directions,
     shown only on Saturdays.
8. Format changed C/C++ files using the repository wrapper and build the explicit
   `x4pro` PlatformIO environment. Do not publish a firmware image from a failed
   or incomplete build.
9. Merge the bootloader at `0x0`, partition table at `0x8000`, and application at
   `0x10000` into one full ESP32-S3 image. Validate all three image markers and
   calculate a SHA-256 checksum.
10. Replace the firmware payload in the sibling `transit-installer` site, run its
   formatter, linter, production build, and browser QA, then publish a new private
   version at the existing installer URL.
11. Push the rebased app branches and `custom-apps` with `--force-with-lease` when
   their published history was rewritten. Never force
   push `xteink`, and never push to `upstream`.
12. Report the upstream version or commit, any conflicts or adaptations made,
    firmware build result, checksum, and installer publication result.

## Safety boundaries

- Rebase and publication are deliberate operations; do not automate them merely
  because upstream changed.
- Refuse to flash builds for ESP32-C3 devices. This custom image targets the
  Xteink X4 Pro's ESP32-S3.
- Preserve user changes and unrelated worktree files.
- Keep generated firmware in `dist/`; do not commit build artifacts to Git.
- If an upstream change makes the custom app unsafe or ambiguous, stop before
  publishing and explain the decision that is needed.

## Build the combined firmware locally

From PowerShell, while `custom-apps` is checked out:

```powershell
.\build-transit-x4pro.ps1
```

The script uses PlatformIO's isolated environment, builds only the ESP32-S3
`x4pro` target, merges and validates the three flash regions, and writes the
untracked image to `dist/crossplay-transit-test-x4pro-full.bin` with its SHA-256
checksum. Set no global Python or PlatformIO environment variables; the script
also enables UTF-8 output to avoid the Windows console failure seen during the
first manual build.

## Manual Git reference

These are the underlying branch operations when they are needed:

```bash
git fetch upstream
git switch xteink
git merge --ff-only upstream/xteink
git push origin xteink

git switch app-transit
git rebase xteink
git merge project-rules
git push --force-with-lease origin app-transit

git switch custom-apps
git rebase xteink
git push --force-with-lease origin custom-apps
```
