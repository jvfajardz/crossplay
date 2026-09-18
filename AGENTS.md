# Xteink X4 Pro application rules

These instructions apply to every task in this repository.

## Start-of-task requirements

Before designing or implementing a custom app:

1. Read `docs/X4PRO_APP_GUIDE.md` and `CONTRIBUTING.md` when it exists on the
   current branch.
2. Inspect the current branch, worktree, remotes, and existing `app-*` branches.
3. Compare the requested behavior with the hard constraints in the guide.
4. Tell the user about any infeasible or high-risk requirement before building
   around it. Offer the smallest viable alternative.
5. Treat “Let's create a new project” as a request for a new independent
   CrossPlay app in this repository. Ask only for the app name and one-sentence
   purpose when they were not supplied.

## Branch model

- Keep `xteink` identical to `upstream/xteink`.
- Keep shared device rules on `project-rules`, based on `xteink`.
- Create each app independently as `app-<name>` from `xteink`, then merge
  `project-rules` into it before app work begins.
- Never base one app branch on another app branch.
- Keep app implementation under `src/apps_local/<name>/` with only minimal
  shared shelf, build, font, or resource wiring.
- Use `custom-apps` only to combine hardware-tested apps into one firmware.
- Do not commit generated firmware or `dist/`.

## Definition of done

- Review a physically calibrated web preview for visual apps before flashing.
- Build only the explicit `x4pro` ESP32-S3 target for device testing.
- Validate the merged bootloader, partition table, application markers, and
  SHA-256 before publishing an installer.
- Do not commit or integrate an app merely because it compiles. Wait for the
  user's hardware confirmation.
- After confirmation, push the standalone app branch, merge it into
  `custom-apps`, rebuild the combined firmware, and update its installer when
  requested.
- If experience changes a durable device constraint, update
  `docs/X4PRO_APP_GUIDE.md` and propagate `project-rules` to active branches.

