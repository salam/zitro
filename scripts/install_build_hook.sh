#!/bin/bash
# Makes arduino-cli run scripts/zitro_build_info.sh in every compile of the sketch on this Mac,
# whether through verify.sh, deploy.sh or a compile by hand, so development builds show their
# build info on the boot splash.
#
#   scripts/install_build_hook.sh              Install (quiet when it is already there)
#   scripts/install_build_hook.sh --uninstall  Remove it again
#
# It goes into each installed esp32 core, next to the core's own platform.txt: a hook line in
# platform.local.txt and the small script zitro_build_hook.sh that the line calls. That is outside
# this repository, and a core update starts a new folder without them, so verify.sh runs this
# script every time.
#
# The hook acts only for sketches inside a worktree of this repository, and runs that worktree's
# scripts/zitro_build_info.sh, so each branch uses its own copy. Other sketches are left alone,
# and git never runs in their folders.

BEGIN="# >>> zitro build info (scripts/install_build_hook.sh)"
END="# <<< zitro build info"
# Paths go in as separate arguments, never into shell code
HOOK='recipe.hooks.sketch.prebuild.90.pattern=/bin/bash "{runtime.platform.path}/zitro_build_hook.sh" "{build.source.path}" "{build.path}"'

# The repository all worktrees share: the hook trusts sketches inside its worktrees only
GIT_COMMON=$(git -C "$(dirname "$0")" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)
REPO=$(dirname "$GIT_COMMON")
if [ "$1" != "--uninstall" ] && [ ! -d "$GIT_COMMON" ]; then
    echo "install_build_hook.sh: not inside a git checkout, no build info hook installed" >&2
    exit 0
fi

wrapper() {
    cat <<EOF
#!/bin/bash
# Installed by scripts/install_build_hook.sh of the Zitro repository; arduino-cli runs it before
# compiling any sketch with this core. It acts only for sketches inside a worktree of
# $REPO
# and never fails the build.
REPO=$(printf '%q' "$REPO")
SRC=\$(cd "\$1" 2>/dev/null && pwd -P) || exit 0
[ -n "\$2" ] || exit 0
# The deepest worktree holding the sketch (the main checkout holds the others in .worktrees/)
TREE=""
while read -r CANDIDATE; do
    CANDIDATE=\$(cd "\$CANDIDATE" 2>/dev/null && pwd -P) || continue
    case "\$SRC/" in
        "\$CANDIDATE"/*) [ \${#CANDIDATE} -gt \${#TREE} ] && TREE="\$CANDIDATE" ;;
    esac
done < <(git -C "\$REPO" worktree list --porcelain 2>/dev/null | sed -n 's/^worktree //p')
[ -n "\$TREE" ] && [ -f "\$TREE/scripts/zitro_build_info.sh" ] || exit 0
/bin/bash "\$TREE/scripts/zitro_build_info.sh" "\$SRC" "\$2"
exit 0
EOF
}

DATA=$(arduino-cli config get directories.data 2>/dev/null)
[ -n "$DATA" ] || DATA="$HOME/Library/Arduino15"

for CORE in "$DATA"/packages/esp32/hardware/esp32/*/; do
    [ -f "$CORE/platform.txt" ] || continue
    FILE="${CORE}platform.local.txt"
    SCRIPT="${CORE}zitro_build_hook.sh"
    if [ "$1" != "--uninstall" ] && [ -f "$FILE" ] && grep -qxF "$HOOK" "$FILE" \
        && [ "$(cat "$SCRIPT" 2>/dev/null)" = "$(wrapper)" ]; then
        continue
    fi
    # Everything else in the file stays; an older version of the block goes
    OTHER=""
    if [ -f "$FILE" ]; then
        OTHER=$(awk -v b="$BEGIN" -v e="$END" '$0 == b { skip = 1 } !skip { print } $0 == e { skip = 0 }' "$FILE")
    fi
    if [ "$1" = "--uninstall" ]; then
        [ -f "$FILE" ] || [ -f "$SCRIPT" ] || continue
        rm -f "$SCRIPT"
        if [ -f "$FILE" ]; then
            if [ -z "$OTHER" ]; then rm -f "$FILE"; else printf '%s\n' "$OTHER" > "$FILE"; fi
        fi
        echo "Removed the build info hook from $CORE"
    else
        wrapper > "$SCRIPT"
        {
            [ -n "$OTHER" ] && printf '%s\n' "$OTHER"
            printf '%s\n%s\n%s\n' "$BEGIN" "$HOOK" "$END"
        } > "$FILE"
        echo "Installed the build info hook in $CORE"
    fi
done
exit 0
