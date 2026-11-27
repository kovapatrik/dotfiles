#!/usr/bin/env bash
# Links the shared Claude config from ~/.claude into every extra profile dir
# (~/.claude-*, used via CLAUDE_CONFIG_DIR). Profiles stay untracked; this runs
# on every apply so new profile dirs get picked up.
set -euo pipefail

shared=(agents commands skills CLAUDE.md statusline.sh)

for profile in "$HOME"/.claude-*/; do
  [ -d "$profile" ] || continue
  profile=${profile%/}
  for item in "${shared[@]}"; do
    src="$HOME/.claude/$item"
    dst="$profile/$item"
    [ -e "$src" ] || continue
    if [ -L "$dst" ]; then
      [ "$(readlink "$dst")" = "$src" ] && continue
      ln -sfn "$src" "$dst"
    elif [ -e "$dst" ]; then
      echo "claude-profiles: skipping $dst (exists and is not a symlink)" >&2
    else
      ln -s "$src" "$dst"
    fi
  done
done
