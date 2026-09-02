set -eu

case "${1:-}" in
    c)
        exec ./build/debug/crapgame-c
        ;;
    cpp)
        exec ./build/debug/crapgame-cpp
        ;;
    *)
        echo "usage: sh scripts/rendercheck-run.sh {c|cpp}" >&2
        exit 2
        ;;
esac
