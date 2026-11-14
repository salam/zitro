#!/bin/bash
# Build info for the boot splash of development builds, so every session can tell which firmware
# is on a shared stick: the newest feature keywords, then the commit and the build time, e.g.
#
#   build, fast, sticks3, voci
#   #9b0a4+, 18:23              ("+": the sketch had uncommitted changes)
#
# The keywords come from the current branch (feature/build-info -> "build") and the merged
# branches before it ("Merge pull request #24 from salam/fix/voci-row-limit" -> "voci").
#
# arduino-cli runs this inside every compile on this Mac, through the hook that
# scripts/install_build_hook.sh puts into the esp32 core (verify.sh keeps it installed):
#
#   zitro_build_info.sh <sketch folder> <build folder>
#
# It writes <build folder>/sketch/zitro_build_info.cpp, which the sketch picks up as
# zitroDevBuildInfo. With ZITRO_RELEASE=1 (installer/rebuild_installer.sh) it writes nothing, and
# the splash shows the logo alone. It never fails the build.

SRC="$1"
BUILD="$2"
OUT="$BUILD/sketch/zitro_build_info.cpp"
MAX_KEYWORDS=4

# A file from an earlier build in the same folder must never outlive this one
rm -f "$OUT"
[ "${ZITRO_RELEASE:-}" = 1 ] && exit 0
[ -d "$SRC" ] && [ -d "$BUILD/sketch" ] || exit 0

# Branch name -> one short word: the first word of its last part, skipping filler words
keyword() {
    local word
    for word in $(echo "${1##*/}" | tr 'A-Z_-' 'a-z  '); do
        case "$word" in
            new|add|fix|more|better|the|a|an|use|make|show) continue ;;
        esac
        word=$(echo "$word" | tr -cd 'a-z0-9' | cut -c1-8)
        [ -n "$word" ] && { echo "$word"; return; }
    done
}

# Branches that only sync, never a feature of their own
is_trunk() {
    case "$1" in
        main|master|public-main|HEAD|origin/*) return 0 ;;
    esac
    return 1
}

branches() {
    local current
    current=$(git -C "$SRC" symbolic-ref --short -q HEAD)
    [ -n "$current" ] && echo "$current"
    # All merges, not only the first-parent line, so features that came in through a
    # "merge main into the branch" count too
    git -C "$SRC" log --merges --date-order --format=%s -n 300 HEAD 2>/dev/null | sed -n \
        -e "s/^Merge pull request #[0-9]* from \([^ ]*\).*/\1/p" \
        -e "s/^Merge branch '\([^']*\)'.*/\1/p"
}

KEYWORDS=""
COUNT=0
while read -r BRANCH; do
    [ -z "$BRANCH" ] && continue
    is_trunk "${BRANCH#*/}" && continue  # PR merges name the branch as owner/branch
    is_trunk "$BRANCH" && continue
    WORD=$(keyword "$BRANCH")
    [ -z "$WORD" ] && continue
    case ", $KEYWORDS, " in *", $WORD, "*) continue ;; esac
    KEYWORDS="${KEYWORDS:+$KEYWORDS, }$WORD"
    COUNT=$((COUNT + 1))
    [ "$COUNT" -ge "$MAX_KEYWORDS" ] && break
done < <(branches)
[ -n "$KEYWORDS" ] || KEYWORDS="dev build"

COMMIT=$(git -C "$SRC" rev-parse --short=5 HEAD 2>/dev/null | tr -cd '0-9a-f')
[ -n "$COMMIT" ] || COMMIT="nogit"
DIRTY=""
# Pathspec "." is the sketch folder (git -C); build output there is gitignored
git -C "$SRC" diff --quiet HEAD -- . 2>/dev/null || DIRTY="+"
[ -z "$(git -C "$SRC" ls-files --others --exclude-standard -- . 2>/dev/null)" ] || DIRTY="+"

INFO="$KEYWORDS\\n#$COMMIT$DIRTY, $(date +%H:%M)"
{
    echo "// Made by scripts/zitro_build_info.sh for this development build; release builds have none."
    echo "extern \"C\" const char zitroDevBuildInfo[] = \"$INFO\";"
} > "$OUT.tmp" && mv -f "$OUT.tmp" "$OUT"
exit 0
