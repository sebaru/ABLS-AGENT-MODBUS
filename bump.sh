#!/bin/bash
# bump.sh - Prepare a release tag and merge trunk into main.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
WORKSPACE_DIR="$(cd "$PROJECT_DIR/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: ./bump.sh <version> [--dry-run]

Examples:
  ./bump.sh 1.0
  ./bump.sh v1.0
  ./bump.sh 1.2.3 --dry-run

Behavior:
  - Requires a clean working tree
  - Works from trunk
  - Creates annotated tag v<version>
  - Creates main from trunk if missing, then merges trunk into main
  - Pushes main and tag to origin

Environment:  - ABLS_DEB_SUITES: optional Debian suite list for build_apt.sh
    (default: "bookworm trixie")
  - ABLS_DEB_ARCHES: optional Debian arch list for build_apt.sh
    (default: "amd64 arm64 armhf")
EOF
}

version_arg=""
dry_run=false

for arg in "$@"; do
  case "$arg" in
    --dry-run)
      dry_run=true
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      if [[ -z "$version_arg" ]]; then
        version_arg="$arg"
      else
        echo "Error: unexpected argument '$arg'"
        usage
        exit 2
      fi
      ;;
  esac
done

if [[ -z "$version_arg" ]]; then
  echo "Error: missing version parameter"
  usage
  exit 2
fi

normalized_version="${version_arg#v}"
if [[ ! "$normalized_version" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
  echo "Error: invalid version '$version_arg' (expected 1.0, 1.2.3, v1.0...)"
  exit 2
fi

release_tag="v${normalized_version}"
DEB_SUITES="${ABLS_DEB_SUITES:-bookworm trixie}"

run_cmd() {
  local cmd="$*"
  echo "+ $cmd"
  if [[ "$dry_run" == "false" ]]; then
    eval "$cmd"
  fi
}

ensure_git_repo() {
  if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: current directory is not a git repository"
    exit 1
  fi
}

ensure_clean_tree() {
  if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Error: working tree is not clean. Commit/stash changes before running bump.sh"
    exit 1
  fi
}

ensure_remote_origin() {
  if ! git remote get-url origin >/dev/null 2>&1; then
    echo "Error: remote 'origin' is not configured"
    exit 1
  fi
}

ensure_branch_exists() {
  local branch="$1"
  if ! git show-ref --verify --quiet "refs/heads/$branch" && \
     ! git show-ref --verify --quiet "refs/remotes/origin/$branch"; then
    echo "Error: branch '$branch' does not exist locally or on origin"
    exit 1
  fi
}

ensure_tag_absent() {
  if git rev-parse "$release_tag" >/dev/null 2>&1; then
    echo "Error: local tag '$release_tag' already exists"
    exit 1
  fi

  if git ls-remote --tags origin "refs/tags/$release_tag" | grep -q "$release_tag"; then
    echo "Error: remote tag '$release_tag' already exists on origin"
    exit 1
  fi
}

initial_branch=""
current_branch() {
  git rev-parse --abbrev-ref HEAD
}

restore_branch() {
  if [[ -n "$initial_branch" ]]; then
    local now
    now="$(current_branch)"
    if [[ "$now" != "$initial_branch" ]]; then
      if git show-ref --verify --quiet "refs/heads/$initial_branch"; then
        run_cmd "git switch $initial_branch"
      fi
    fi
  fi
}

trap restore_branch EXIT

ensure_git_repo
ensure_remote_origin
ensure_clean_tree

initial_branch="$(current_branch)"

echo "Preparing release for $release_tag"
echo "Initial branch: $initial_branch"
if [[ "$dry_run" == "true" ]]; then
  echo "Dry-run enabled: commands are displayed but not executed"
fi

run_cmd "git fetch origin --prune"

ensure_branch_exists trunk

run_cmd "git switch trunk"
run_cmd "git pull --ff-only origin trunk"

ensure_tag_absent
run_cmd "git tag -a $release_tag -m 'TAG: Create tag $release_tag.'"
run_cmd "./build.sh"
run_cmd "./build_rpm.sh"
for deb_suite in $DEB_SUITES; do
  run_cmd "./build_apt.sh --dist $deb_suite"
done

if git show-ref --verify --quiet refs/remotes/origin/main; then
  if git show-ref --verify --quiet refs/heads/main; then
    run_cmd "git switch main"
  else
    run_cmd "git switch -c main --track origin/main"
  fi
  run_cmd "git pull --ff-only origin main"
else
  run_cmd "git switch -c main trunk"
  run_cmd "git push -u origin main"
fi

run_cmd "git merge --no-ff -m 'Create $release_tag' trunk"
run_cmd "git push origin main"
run_cmd "git push origin $release_tag"

echo "Release flow completed for $release_tag"
echo "Rollback hints if needed:"
echo "  - delete local tag:   git tag -d $release_tag"
echo "  - delete remote tag:  git push origin --delete $release_tag"
echo "  - inspect main state: git log --oneline --decorate -n 10 main"
